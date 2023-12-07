#!/usr/bin/env python3

import enum

import basetypes

import command_server


class i2c_device_sen54_t(basetypes.i2c_device_t):

    SEN54_ADDR   = 0x69

    SEN54_CMD_MEASUREMENT_START_ALL         = 0x2100
    SEN54_CMD_MEASUREMENT_START_RHT_GAS     = 0x3700
    SEN54_CMD_MEASUREMENT_STOP              = 0x0401
    SEN54_CMD_DATAREADY                     = 0x0202
    SEN54_CMD_MEASURED_VALUES               = 0xC403
    SEN54_CMD_TEMPERATURE_COMP              = 0xB260
    SEN54_CMD_WARM_START                    = 0xC660
    SEN54_CMD_VOC_ALGORITHM_TUNING          = 0xD060
    SEN54_CMD_NOX_ALGORITHM_TUNING          = 0xE160
    SEN54_CMD_RELHUM_TEMP_ACCELERATION_MODE = 0xF760
    SEN54_CMD_VOC_ALGORITHM_STATE           = 0x8161
    SEN54_CMD_FAN_CLEANING_START            = 0x0756
    SEN54_CMD_FAN_CLEANING_AUTO_INTERVAL    = 0x0480
    SEN54_CMD_PRODUCT_NAME                  = 0x14D0
    SEN54_CMD_SERIAL_NAME                   = 0x33D0
    SEN54_CMD_FIRMWARE_VERSION              = 0x00D1
    SEN54_CMD_DEVICE_STATUS                 = 0x06D2
    SEN54_CMD_DEVICE_STATUS_CLEAR           = 0x10D2
    SEN54_CMD_RESET                         = 0x04D3

    SEN54_CMDS = {
        SEN54_CMD_MEASUREMENT_START_ALL         : 0x00 ,
        SEN54_CMD_MEASUREMENT_START_RHT_GAS     : 0x00 ,
        SEN54_CMD_MEASUREMENT_STOP              : 0x00 ,
        SEN54_CMD_DATAREADY                     : 0x00 ,
        SEN54_CMD_MEASURED_VALUES               : 0x00 ,
        SEN54_CMD_TEMPERATURE_COMP              : 0x00 ,
        SEN54_CMD_WARM_START                    : 0x00 ,
        SEN54_CMD_VOC_ALGORITHM_TUNING          : 0x00 ,
        SEN54_CMD_NOX_ALGORITHM_TUNING          : 0x00 ,
        SEN54_CMD_RELHUM_TEMP_ACCELERATION_MODE : 0x00 ,
        SEN54_CMD_VOC_ALGORITHM_STATE           : 0x00 ,
        SEN54_CMD_FAN_CLEANING_START            : 0x00 ,
        SEN54_CMD_FAN_CLEANING_AUTO_INTERVAL    : 0x00 ,
        SEN54_CMD_PRODUCT_NAME                  : 0x53454e3534,
        SEN54_CMD_SERIAL_NAME                   : 0x3345364438383144343830414344433300,
        SEN54_CMD_FIRMWARE_VERSION              : 0x00 ,
        SEN54_CMD_DEVICE_STATUS                 : 0x00 ,
        SEN54_CMD_DEVICE_STATUS_CLEAR           : 0x00 ,
        SEN54_CMD_RESET                         : 0x00 ,
        }

    SEN54_MEASUREMENTS = enum.Enum('sen54_measurement_t',
                            ["PM1"        ,
                             "PM2_5"      ,
                             "PM4"        ,
                             "PM10"       ,
                             "HUMIDITY"   ,
                             "TEMPERATURE",
                             "VOC"        ,
                             "NOX"        ]
                            )

    SEN54_DEFAULT_MEASUREMENT_VALUES = {
        SEN54_MEASUREMENTS.PM1.value         : 1.1   ,
        SEN54_MEASUREMENTS.PM2_5.value       : 2.2   ,
        SEN54_MEASUREMENTS.PM4.value         : 3.3   ,
        SEN54_MEASUREMENTS.PM10.value        : 4.4   ,
        SEN54_MEASUREMENTS.HUMIDITY.value    : 45.1  ,
        SEN54_MEASUREMENTS.TEMPERATURE.value : 22.5  ,
        SEN54_MEASUREMENTS.VOC.value         : 5.5   ,
        SEN54_MEASUREMENTS.NOX.value         : 6.6   ,
        }

    SEN54_MEASUREMENT_SCALE_FACTORS = {
        SEN54_MEASUREMENTS.PM1.value         : 10    ,
        SEN54_MEASUREMENTS.PM2_5.value       : 10    ,
        SEN54_MEASUREMENTS.PM4.value         : 10    ,
        SEN54_MEASUREMENTS.PM10.value        : 10    ,
        SEN54_MEASUREMENTS.HUMIDITY.value    : 100   ,
        SEN54_MEASUREMENTS.TEMPERATURE.value : 200   ,
        SEN54_MEASUREMENTS.VOC.value         : 10    ,
        SEN54_MEASUREMENTS.NOX.value         : 10    ,
        }

    SEN54_MEASUREMENT_SHIFT = {
        SEN54_MEASUREMENTS.PM1.value         : 0 *8  ,
        SEN54_MEASUREMENTS.PM2_5.value       : 3 *8  ,
        SEN54_MEASUREMENTS.PM4.value         : 6 *8  ,
        SEN54_MEASUREMENTS.PM10.value        : 9 *8  ,
        SEN54_MEASUREMENTS.HUMIDITY.value    : 12*8  ,
        SEN54_MEASUREMENTS.TEMPERATURE.value : 15*8  ,
        SEN54_MEASUREMENTS.VOC.value         : 18*8  ,
        SEN54_MEASUREMENTS.NOX.value         : 21*8  ,
        }

    def __init__(self, **kwargs):
        super().__init__(self.SEN54_ADDR, self.SEN54_CMDS, cmd_size=2)
        pm1             = kwargs.get("pm1",         self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.PM1.value         ])
        pm2_5           = kwargs.get("pm2_5",       self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.PM2_5.value       ])
        pm4             = kwargs.get("pm4",         self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.PM4.value         ])
        pm10            = kwargs.get("pm10",        self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.PM10.value        ])
        humidity        = kwargs.get("humidity",    self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.HUMIDITY.value    ])
        temperature     = kwargs.get("temperature", self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.TEMPERATURE.value ])
        voc             = kwargs.get("voc",         self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.VOC.value         ])
        nox             = kwargs.get("nox",         self.SEN54_DEFAULT_MEASUREMENT_VALUES[self.SEN54_MEASUREMENTS.NOX.value         ])

        self.pm1            = pm1
        self.pm2_5          = pm2_5
        self.pm4            = pm4
        self.pm10           = pm10
        self.humidity       = humidity
        self.temperature    = temperature
        self.voc            = voc
        self.nox            = nox

    @staticmethod
    def crc(data):
        data = data & 0xFFFF
        data_arr = [(data >> (i-1)*8) & 0xFF for i in range(2, 0, -1)]
        crc = 0xFF
        for i in range(0, 2):
            crc ^= data_arr[i]
            for bit in range(8, 0, -1):
                if crc & 0x80:
                    crc = (crc << 1) ^ 0x31
                else:
                    crc = crc << 1
        return crc & 0xFF

    def _get_measurement(self, measurement):
        raw = int(self.SEN54_CMDS[self.SEN54_CMD_MEASURED_VALUES])
        shift = self.SEN54_MEASUREMENT_SHIFT[measurement.value]
        crc_shift = shift + 2*8

        raw_value = ((raw >> shift) & 0xFF) << (1*8)
        raw_value +=  (raw >> shift+1*8) & 0xFF
        crc = (raw >> crc_shift) & 0xFF

        calc_crc = self.crc(raw_value)
        assert calc_crc == crc, f"CRC fails! (0x{calc_crc:02X} != 0x{crc:02X})"
        scale = self.SEN54_MEASUREMENT_SCALE_FACTORS[measurement.value]
        value = float(raw_value) / scale
        return value

    def _set_measurement(self, measurement, val):
        scale = self.SEN54_MEASUREMENT_SCALE_FACTORS[measurement.value]
        new_raw = int(val * scale)
        new_crc = self.crc(new_raw)
        shift = self.SEN54_MEASUREMENT_SHIFT[measurement.value]
        crc_shift = shift + 2*8

        mem = int(self.SEN54_CMDS[self.SEN54_CMD_MEASURED_VALUES])

        mem &= ~(0xFF << shift)
        mem |= ((new_raw >> (1*8)) & 0xFF) << shift

        mem &= ~(0xFF << (shift+1*8))
        mem |= (new_raw & 0xFF) << (shift+1*8)

        mem &= ~(0xFF << crc_shift)
        mem |= (new_crc & 0xFF) << crc_shift

        self.SEN54_CMDS[self.SEN54_CMD_MEASURED_VALUES] = mem

    def __getattr__(self, attr):
        for measurement in self.SEN54_MEASUREMENTS:
            if attr.upper() == measurement.name:
                return self._get_measurement(measurement)
        return super().__getattribute__(attr)

    def __setattr__(self, attr, val):
        for measurement in self.SEN54_MEASUREMENTS:
            if attr.upper() == measurement.name:
                self._set_measurement(measurement, val)
                return
        super().__setattr__(attr, val)

    def return_obj(self):
        params = []
        for measurement in self.SEN54_MEASUREMENTS:
            params.append(
                command_server.fake_param_t(
                    measurement.name,
                    getf=lambda: self._get_measurement(measurement),
                    setf=lambda: self._set_measurement(measurement)
                    )
                )
        return command_server.fake_dev_t("SEN54", params)


def main():
    sen54 = i2c_device_sen54_t()
    val = 0xBEEF
    print(f"CRC(0x{val:04X}) = 0x{sen54.crc(val):02X}")
    print(f"PM 1.0      = {sen54.pm1}")
    print(f"PM 2.5      = {sen54.pm2_5}")
    print(f"PM 4.0      = {sen54.pm4}")
    print(f"PM 10.0     = {sen54.pm10}")
    print(f"TEMPERATURE = {sen54.temperature}")
    print(f"HUMIDITY    = {sen54.humidity}")
    print(f"VOC         = {sen54.voc}")
    print(f"NOX         = {sen54.nox}")
    sen54.pm1         = 11.1
    sen54.pm2_5       = 12.2
    sen54.pm4         = 13.3
    sen54.pm10        = 14.4
    sen54.temperature = 1.1
    sen54.humidity    = 30.1
    sen54.voc         = 15.5
    sen54.nox         = 16.6
    print(f"PM 1.0      = {sen54.pm1}")
    print(f"PM 2.5      = {sen54.pm2_5}")
    print(f"PM 4.0      = {sen54.pm4}")
    print(f"PM 10.0     = {sen54.pm10}")
    print(f"TEMPERATURE = {sen54.temperature}")
    print(f"HUMIDITY    = {sen54.humidity}")
    print(f"VOC         = {sen54.voc}")
    print(f"NOX         = {sen54.nox}")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
