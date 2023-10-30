
#include "Arduino.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include <pb_common.h>
#include "ibeam.pb.h"

#include <WiFi.h>

const char *ssid = "BachiNet";
const char *password = "";

const uint16_t port = 8090;
const char *host = "192.168.0.111";

/* This is a simple TCP client that connects to port 1234 and prints a list
 * of files in a given directory.
 *
 * It directly deserializes and serializes messages from network, minimizing
 * memory use.
 * 
 * For flexibility, this example is implemented using posix api.
 * In a real embedded system you would typically use some other kind of
 * a communication and filesystem layer.
 */

/* This callback function will be called once for each filename received
 * from the server. The filenames will be printed out immediately, so that
 * no memory has to be allocated for them.

bool ListFilesResponse_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_iter_t *field)
{
    PB_UNUSED(ostream);
    if (istream != NULL && field->tag == ListFilesResponse_file_tag)
    {
        FileInfo fileinfo = {};

        if (!pb_decode(istream, FileInfo_fields, &fileinfo))
            return false;

        printf("%-10lld %s\n", (long long)fileinfo.inode, fileinfo.name);
    }
    
    return true;
}
 */
/* This function sends a request to socket 'fd' to list the files in
 * directory given in 'path'. The results received from server will
 * be printed to stdout.


bool listdir(int fd, char *path)
{
    // Construct and send the request to server 
    {
        ListFilesRequest request = {};
        pb_ostream_t output = pb_ostream_from_socket(fd);
        
        // In our protocol, path is optional. If it is not given,
         // the server will list the root directory. 
        if (path == NULL)
        {
            request.has_path = false;
        }
        else
        {
            request.has_path = true;
            if (strlen(path) + 1 > sizeof(request.path))
            {
                fprintf(stderr, "Too long path.\n");
                return false;
            }
            
            strcpy(request.path, path);
        }
        
        // Encode the request. It is written to the socket immediately
         // through our custom stream. 
        if (!pb_encode_delimited(&output, ListFilesRequest_fields, &request))
        {
            fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
            return false;
        }
    }
    
    // Read back the response from server
    {
        ListFilesResponse response = {};
        pb_istream_t input = pb_istream_from_socket(fd);
        
        if (!pb_decode_delimited(&input, ListFilesResponse_fields, &response))
        {
            fprintf(stderr, "Decode failed: %s\n", PB_GET_ERROR(&input));
            return false;
        }
        
        // If the message from server decodes properly, but directory was
        // not found on server side, we get path_error == true. 
        if (response.path_error)
        {
            fprintf(stderr, "Server reported error.\n");
            return false;
        }
    }
    
    return true;
}
 */

void setup()
{

    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("...");
    }

    Serial.print("WiFi connected with IP: ");
    Serial.println(WiFi.localIP());
}

bool encode_unionmessage(pb_ostream_t *stream, const pb_msgdesc_t *messagetype, void *message)
{
    pb_field_iter_t iter;

    if (!pb_field_iter_begin(&iter, IbeamMessageData_fields, message))
        return false;

    do
    {
        if (iter.submsg_desc == messagetype)
        {
            // This is our field, encode the message using it.
            if (!pb_encode_tag_for_field(stream, &iter))
                return false;

            return pb_encode_submessage(stream, messagetype, message);
        }
    } while (pb_field_iter_next(&iter));

    // Didn't find the field for messagetype
    return false;
}

bool write_message_bytes(pb_ostream_t *stream, const pb_field_iter_t *field, void *const *arg)
{
    uint8_t *data = (uint8_t *)*arg;
    Serial.printf("Encode bytes %d\n", data[0]);

    //uint8_t someBytes[4] = {8, 5, 16, 22};
    if (!pb_encode_tag_for_field(stream, field))
        return false;

    return pb_encode_string(stream, &data[1], data[0]);
}

bool decode_message_bytes(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    while (stream->bytes_left)
    {
        uint64_t value;
        if (!pb_decode_varint(stream, &value))
            return false;
        printf("%lld\n", value);
    }
    return true;
}

/* This function reads manually the first tag from the stream and finds the
 * corresponding message type. It doesn't yet decode the actual message.
 *
 * Returns a pointer to the MsgType_fields array, as an identifier for the
 * message type. Returns null if the tag is of unknown type or an error occurs.
 */
const pb_msgdesc_t *decode_unionmessage_type(pb_istream_t *stream)
{
    pb_wire_type_t wire_type;
    uint32_t tag;
    bool eof;

    while (pb_decode_tag(stream, &wire_type, &tag, &eof))
    {
        if (wire_type == PB_WT_STRING)
        {
            pb_field_iter_t iter;
            if (pb_field_iter_begin(&iter, IbeamMessageData_fields, NULL) &&
                pb_field_iter_find(&iter, tag))
            {
                /* Found our field. */
                return iter.submsg_desc;
            }
        }

        /* Wasn't our field.. */
        pb_skip_field(stream, wire_type);
    }

    return NULL;
}

bool decode_unionmessage_contents(pb_istream_t *stream, const pb_msgdesc_t *messagetype, void *dest_struct)
{
    pb_istream_t substream;
    bool status;
    if (!pb_make_string_substream(stream, &substream))
        return false;

    status = pb_decode(&substream, messagetype, dest_struct);
    pb_close_string_substream(stream, &substream);
    return status;
}

void loop()
{
    WiFiClient client;

    if (!client.connect(host, port))
    {

        Serial.println("Connection to host failed");

        delay(1000);
        return;
    }

    Serial.println("Connected to server successful!");
    {

        uint8_t buffer[20];
        pb_ostream_t stream = pb_ostream_from_buffer(&buffer[1], sizeof(buffer) - 1);
        //pb_istream_t instream = pb_istream_from(buffer, sizeof(buffer));

        GetRequest get = {5};

        bool status = encode_unionmessage(&stream, GetRequest_fields, &get);
        Serial.printf("Status %d %d\n", status, stream.bytes_written);

        buffer[0] = stream.bytes_written;
        IbeamMessage mdata = {};
        mdata.data.funcs.encode = &write_message_bytes;
        mdata.data.arg = &buffer;
        uint8_t final_buffer[25];
        pb_ostream_t final_stream = pb_ostream_from_buffer(final_buffer, sizeof(final_buffer));

        status = pb_encode(&final_stream, IbeamMessage_fields, &mdata);
        Serial.printf("Status %d %d\n", status, stream.bytes_written);

        //client.print("Hello from ESP32!");
        Serial.print("Final Buffer:");
        for (int i = 0; i < final_stream.bytes_written; i++)
        {
            Serial.print(final_buffer[i]);
            Serial.print(" ");
        }
        Serial.print(" X\n");

        client.write(&final_buffer[0], final_stream.bytes_written);

        Serial.println("Waiting for response...");
    }

    uint8_t inBuffer[100];
    pb_istream_t decodestream = pb_istream_from_buffer(inBuffer, sizeof(inBuffer));
    //message.data.funcs.decode = &decode_message_bytes;

    int maxloops = 0;
    //wait for the server's reply to become available
    while (!client.available() && maxloops < 3000)
    {
        maxloops++;
        delay(1); //delay 1 msec
    }
    if (client.available() > 0)
    {
        //read back one line from the server
        int number = client.read(&inBuffer[0], sizeof(inBuffer));
        for (int i = 0; i < number; i++)
        {
            Serial.print(inBuffer[i]);
            Serial.print(" ");
        }
        Serial.println();
        pb_wire_type_t wire_type;
        uint32_t tag;
        bool eof;

        // Next filed it is a Bytes Field!
        bool status = pb_decode_tag(&decodestream, &wire_type, &tag, &eof);
        uint64_t datalength;
        status = pb_decode_varint(&decodestream, &datalength);

        IbeamMessageData messagedata = IbeamMessageData_init_default;

        status = pb_decode(&decodestream, IbeamMessageData_fields, &messagedata);

        Serial.printf("Incoming message length bytes %d\n", datalength);
        //Serial.printf("Incoming message data bytes %d\n", message.data);
        //Serial.printf("Bytes Left in stream  %d\n", decodestream.bytes_left);

        Serial.printf("Type  %d\n", messagedata.which_data);

        if (messagedata.which_data == IbeamMessageData_param_tag)
        {
            Serial.printf("Incoming message type Param response !!!\n");
        }
        else if (messagedata.which_data == IbeamMessageData_get_tag)
        {
            Serial.printf("Incoming message type GET !!!\n");
        }
        else if (messagedata.which_data == IbeamMessageData_set_tag)
        {
            Serial.printf("Incoming message type SET  !!!\n");
        }
        else
        {
            Serial.println("different type ");
        }
    }

    Serial.println("Disconnecting...");
    client.stop();

    delay(5000);
}
