// Decode decodes an array of bytes into an object.
//  - fPort contains the LoRaWAN fPort number
//  - bytes is an array of bytes, e.g. [225, 230, 255, 0]
//  - variables contains the device variables e.g. {"calibration": "3.5"} (both the key / value are of type string)
// The function must return an object, e.g. {"temperature": 22.5}

function Decode_u8(bytes, pos)
{
    return bytes[pos];
}


function Decode_u16(bytes, pos)
{
    return (bytes[pos + 1] << 8) | bytes[pos];
}


function Decode_u32(bytes, pos)
{
    return (Decode_u16(bytes, pos + 2) << 16) | Decode_u16(bytes, pos);
}


function Decode_u64(bytes, pos)
{
    return (Decode_u32(bytes, pos + 4) << 32 | Decode_u32(bytes, pos));
}


function Decode_i8(bytes, pos)
{
    var UINT8_MAX = 256;
    var val;
    pos, val = Decode_u8(bytes, pos);
    if (val > UINT8_MAX / 2)
    {
        val -= UINT8_MAX;
    }
    return (pos, val);
}


function Decode_i16(bytes, pos)
{
    var UINT16_MAX = 0xFFFF;
    var val;
    pos, val = Decode_u16(bytes, pos);
    if (val & 0x8000)
    {
        val -= (UINT16_MAX + 1);
    }
    return (pos, val);
}


function Decode_i32(bytes, pos)
{
    var UINT32_MAX = 0xFFFFFFFF;
    var val;
    pos, val = Decode_u32(bytes, pos);
    if (val > UINT32_MAX / 2)
    {

        val -= (UINT32_MAX + 1);
    }
    return (pos, val);
}


function Decode_i64(bytes, pos)
{
    var UINT64_MAX = 0xFFFFFFFFFFFFFFFF;
    var val;
    pos, val = Decode_u64(bytes, pos);
    if (val > UINT64_MAX / 2)
    {
        val -= (UINT64_MAX - 1);
    }
    return (pos, val);
}


function Decode_float(bytes, pos)
{
    return Decode_i32(bytes, pos) / 1000;
}


function Decode_double(bytes, pos)
{
    return Decode_i64(bytes, pos) / 1000;
}


function Decode_string(bytes, pos)
{
    var val = "";
    for (var i = 0; i < 8; i++)
    {
        val += String.fromCharCode(bytes[pos+i]);
    }
    return val;
}

function Decode_value(value_type, bytes, pos)
{
    if (value_type == 0x01)
    {
      return Decode_u8(bytes, pos);
    }
    if (value_type == 0x02)
    {
      return Decode_u16(bytes, pos);
    }
    if (value_type == 0x03)
    {
      return Decode_u32(bytes, pos);
    }
    if (value_type == 0x04)
    {
      return Decode_u64(bytes, pos);
    }
    if (value_type == 0x11)
    {
      return Decode_i8(bytes, pos);
    }
    if (value_type == 0x12)
    {
      return Decode_i16(bytes, pos);
    }
    if (value_type == 0x13)
    {
      return Decode_i32(bytes, pos);
    }
    if (value_type == 0x14)
    {
      return Decode_i64(bytes, pos);
    }
    if (value_type == 0x15)
    {
      return Decode_float(bytes, pos);
    }
    if (value_type == 0x16)
    {
      return Decode_double(bytes, pos);
    }
    if (value_type == 0x20)
    {
      return Decode_string(bytes, pos);
    }
    else
    {
      return false;
    }
}



function Value_sizes(value_type)
{
    if (value_type == 0x01)
    {
      return 1;
    }
    if (value_type == 0x02)
    {
      return 2;
    }
    if (value_type == 0x03)
    {
      return 4;
    }
    if (value_type == 0x04)
    {
      return 8;
    }
    if (value_type == 0x11)
    {
      return 1;
    }
    if (value_type == 0x12)
    {
      return 2;
    }if (value_type == 0x13)
    {
      return 4;
    }
    if (value_type == 0x14)
    {
      return 8;
    }
    if (value_type == 0x15)
    {
      return 4;
    }
    if (value_type == 0x16)
    {
      return 8;
    }
    if (value_type == 0x20)
    {
      return 8;
    }
    else
    {
      return false;
    }
}

function decodeUplink(bytes)
{
    var pos = 0;
    var data = {};

    var protocol_version = bytes.bytes[pos++];

    if (protocol_version != 1 && protocol_version != 2)
    {
        return data;
    }

    var name;
    while(pos < bytes.bytes.length)
    {
        name = "";
        for (var i = 0; i < 4; i++)
        {
            if (bytes.bytes[pos] != 0)
            {
                name += String.fromCharCode(bytes.bytes[pos]);
            }
            pos++;
        }
        var data_type = bytes.bytes[pos++];
        var value_type, func;
        var mean, min, max;
        while (pos < bytes.bytes.length) {
          if (data_type == 1)
          {
              value_type = bytes.bytes[pos++];
              mean = Decode_value(value_type, bytes.bytes, pos);
              pos += Value_sizes(value_type);
              data[name] = mean;
              break;
          }
              // Multiple measurement
          else if (data_type == 2)
          {
              value_type = bytes.bytes[pos++];
              mean = Decode_value(value_type, bytes.bytes, pos);
              pos += Value_sizes(value_type);
              data[name] = mean;

              value_type = bytes.bytes[pos++];
              min = Decode_value(value_type, bytes.bytes, pos);
              pos += Value_sizes(value_type);
              data[name+"_min"] = min;

              value_type = bytes.bytes[pos++];
              max = Decode_value(value_type, bytes.bytes, pos);
              pos += Value_sizes(value_type);
              data[name+"_max"] = max;
              break;
          }
          else
          {
              return data;
          }
        }
    }
  return {
    data: data
  };
}
