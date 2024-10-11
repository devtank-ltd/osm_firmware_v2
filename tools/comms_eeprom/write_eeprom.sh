#!/usr/bin/env bash

syntax_exit () {
    echo "$0 I2C_DEV LW/WIFI/POE";
    exit -1;
}

I2C_DEV=$1
COMMS_TYPE=$2

if [ "$#" -lt "2" ]; then
    syntax_exit
fi

if [ "${COMMS_TYPE}" == "LW" ]; then
    COMMS_TYPE_NUM="01";
elif [ "${COMMS_TYPE}" == "WIFI" ]; then
    COMMS_TYPE_NUM="02";
elif [ "${COMMS_TYPE}" == "POE" ]; then
    COMMS_TYPE_NUM="03";
else
    syntax_exit
fi

EEPROM_MEM_VERSION="01"
DEV_ADDR=0x50
REG_VALS="${EEPROM_MEM_VERSION} 02"

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
print(f'{(crc & 0xFF): 02x} {(crc >> 8): 02x}')
")

REG_VALS="${REG_VALS} ${CRC}"

set_reg () {
    i2cset -y "${I2C_DEV}" "${DEV_ADDR}" "$1" "0x$2";
}

REG_ADDR="0";

for v in ${REG_VALS}; do
    set_reg ${REG_ADDR} ${v};
    REG_ADDR=$((REG_ADDR + 1));
done

echo "Wrote EEPROM for ${COMMS_TYPE}"
