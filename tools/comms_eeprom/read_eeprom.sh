#!/usr/bin/env bash

syntax_exit () {
    echo "$0 I2C_DEV";
    exit -1;
}

I2C_DEV=$1

if [ "$#" -lt "1" ]; then
    syntax_exit
fi

EEPROM_MEM_VERSION="01"
DEV_ADDR=0x50

REG="0"

get_reg () {
    i2cget -y "${I2C_DEV}" "${DEV_ADDR}" "$1";
}

MEMV=$(get_reg 0)
if [ "${MEMV}" != "0x${EEPROM_MEM_VERSION}" ]; then
    echo "Bad mem version";
    exit 1
fi

COMMS_TYPE_NUM=$(get_reg 1)

REG_VALS="${MEMV} ${COMMS_TYPE_NUM}"

CRC=$(python3 -c "
buf = [ int(v, 16) for v in '${REG_VALS}'.split()]
crc = 0xFFFF
for i in range(len(buf)):
    crc ^= buf[i]
    for j in range(8, 0, -1):
        if (crc & 0x1) != 0:
            crc >>= 1
            crc ^= 0xA001
        else:
            crc >>= 1
print(f'0x{(crc & 0xFF):02x} 0x{(crc >> 8):02x}')
")

CRCCMP="$(get_reg 2) $(get_reg 3)"

if [ "${CRCCMP}" != "${CRC}" ]; then
    echo "Bad CRC";
    exit 2;
fi

if [ "${COMMS_TYPE_NUM}" == "0x01" ]; then
    echo "LW";
elif [ "${COMMS_TYPE_NUM}" == "0x02" ]; then
    echo "WIFI";
elif [ "${COMMS_TYPE_NUM}" == "0x03" ]; then
    echo "POE";
fi
