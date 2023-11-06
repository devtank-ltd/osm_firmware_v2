const protocol = require("./cs_protocol");


function str_to_byte_arr(string_in)
{
    var byte_arr = Buffer.from(string_in, "hex");
    return byte_arr.toJSON().data;
}

var bytes = str_to_byte_arr(process.argv[2]);
console.log(protocol.Decode(0, bytes, 0));
