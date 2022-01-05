// Decode decodes an array of bytes into an object.
//  - fPort contains the LoRaWAN fPort number
//  - bytes is an array of bytes, e.g. [225, 230, 255, 0]
//  - variables contains the device variables e.g. {"calibration": "3.5"} (both the key / value are of type string)
// The function must return an object, e.g. {"temperature": 22.5}
function Decode(fPort, bytes, variables)
{
    var pos = 0;
    var obj = {};

    var protocol_version = bytes[pos++];

    if (protocol_version != 1)
    {
        return obj;
    }

    var value_type_sizes = { 0x01 : 1 ,
                             0x02 : 2 ,
                             0x03 : 4 ,
                             0x04 : 8 ,
                             0x11 : 1 ,
                             0x12 : 2 ,
                             0x13 : 4 ,
                             0x14 : 8 };


    var name;
    while(pos < bytes.length)
    {
        name = "";
        for (var i = 0; i < 4; i++)
        {
            if (bytes[pos] != 0)
            {
                name += String.fromCharCode(bytes[pos]);
            }
            pos++;
        }
        var data_type = bytes[pos++];
        var value_size;
        var mean, min, max;
        switch (data_type)
        {
            // Single measurement
            case 1:
                value_type = value_type_sizes[bytes[pos++]];
                mean = 0;
                for (var i = 0; i < value_type; i++)
                {
                    mean = (mean << 8) | bytes[pos++];
                }
                obj[name] = mean;
                break;
            // Multiple measurement
            case 2:
                value_size = value_type_sizes[bytes[pos++]];
                mean = 0;
                for (var i = 0; i < value_size; i++)
                {
                    mean = (mean << 8) | bytes[pos++];
                }
                obj[name] = mean;

                value_size = value_type_sizes[bytes[pos++]];
                min = 0;
                for (var i = 0; i < value_size; i++)
                {
                    min = (min << 8) | bytes[pos++];
                }
                obj[name+"_min"] = min;

                value_size = value_type_sizes[bytes[pos++]];
                max = 0;
                for (var i = 0; i < value_size; i++)
                {
                    max = (max << 8) | bytes[pos++];
                }
                obj[name+"_max"] = max;
                break;
            default:
                return obj;
        }
    }
    return obj;
}

// Encode encodes the given object into an array of bytes.
//  - fPort contains the LoRaWAN fPort number
//  - obj is an object, e.g. {"temperature": 22.5}
//  - variables contains the device variables e.g. {"calibration": "3.5"} (both the key / value are of type string)
// The function must return an array of bytes, e.g. [225, 230, 255, 0]
function Encode(fPort, obj, variables)
{
    var MAX_LENGTH = 56;
    var topics = Object.keys(obj);
    var bytes = [];

    if (topics.length > 1)
    {
        return bytes;
    }

    var name = topics[0];
    var payload = obj[name];

    if (name.length > 4)
    {
        return bytes;
    }

    // 1 For protocol version, 4 for name length, X for payload length
    if (1 + 4 + payload.length > MAX_LENGTH)
    {
        return bytes;
    }

    // Protocol version
    bytes.push(1);

    // Send name
    for (var i = 0; i < 4; i++)
    {
        if (name.length > i)
        {
            bytes.push(name.charCodeAt(i));
        }
        else
        {
            bytes.push(0);
        }
    }

    // Send Payload
    for (var i = 0; i < payload.length; i++)
    {
        bytes.push(payload.charCodeAt(i));
    }

    return bytes;
}

console.log(Decode(0, [1, 80, 77, 49, 48, 2, 1, 0, 2, 0, 0, 2, 0, 0, 80, 77, 50, 53, 2, 1, 0, 2, 0, 0, 2, 0, 0, 67, 67, 49, 0, 2, 2, 174, 49, 2, 0, 0, 2, 0, 0, 0], 0));
console.log(Encode(0, {"CMD" : "How are you?"}, 0));
