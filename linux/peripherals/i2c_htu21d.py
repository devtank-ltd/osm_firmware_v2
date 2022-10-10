#!/usr/bin/env python3


from i2c_basetypes import i2c_device_t


class i2c_device_htu21d_t(i2c_device_t):
    HTU21D_ADDR   = 0x40

    HTU21D_HOLD_TRIG_TEMP_MEAS = 0xE3
    HTU21D_HOLD_TRIG_HUMI_MEAS = 0xE5
    HTU21D_TRIG_TEMP_MEAS      = 0xF3
    HTU21D_TRIG_HUMI_MEAS      = 0xF5
    HTU21D_WRITE_USER_REG      = 0xE6
    HTU21D_READ_USER_REG       = 0xE7
    HTU21D_SOFT_RESET          = 0xFE

    HTU21D_CMDS   = {HTU21D_HOLD_TRIG_TEMP_MEAS: 0x63b87e, # Temperature = 21.590degC
                     HTU21D_HOLD_TRIG_HUMI_MEAS: 0x741ee4, # Humidity = 50.160%
                     HTU21D_TRIG_TEMP_MEAS     : 0x0777,
                     HTU21D_TRIG_HUMI_MEAS     : 0x0666,
                     HTU21D_WRITE_USER_REG     : 0x0555,
                     HTU21D_READ_USER_REG      : 0x0444,
                     HTU21D_SOFT_RESET         : 0x0333}

    def __init__(self):
        super().__init__(self.HTU21D_ADDR, self.HTU21D_CMDS)

    def _crc(self, val:int):
        val_hex = hex(val)[2:]
        len_hex = len(val_hex)
        if len_hex % 2:
            val_hex = "0" + val_hex
            len_hex += 1
        mem = [int(val_hex[2*i:2*i+2], 16) for i in range(0, int(len_hex/2))]
        crc = 0x00
        for byte in mem:
            crc ^= byte
            for i in range(1, 9):
                if crc & 0x80:
                    crc <<= 1
                    crc ^= 0x131
                else:
                    crc <<= 1
        return crc

    def _St_2_T(self, St):
        return -46.85 + 175.72 * (St / (2 ** 16))

    def _T_2_St(self, T):
        return int((2 ** 16) * (T + 46.85) / 175.72)

    @property
    def temperature(self):
        return self._St_2_T(self._cmds[self.HTU21D_HOLD_TRIG_TEMP_MEAS] >> 8)

    @temperature.setter
    def temperature(self, new_T:float):
        curr_hum = self.humidity
        new_St = self._T_2_St(float(new_T))
        self._cmds[self.HTU21D_HOLD_TRIG_TEMP_MEAS] = (new_St << 8) | self._crc(new_St)
        self.humidity = curr_hum

    def _Sh_2_H(self, Sh):
        return -6 + 125 * (Sh / (2 ** 16))

    def _H_2_Sh(self, H):
        return int((2 ** 16) * (H + 6) / 125)

    def _H_f_T(self):
        return - 0.15 * (25 - self.temperature)

    @property
    def humidity(self):
        """ Humidity is calculated in two parts. First the count is
        converted to a humidity, second the temperature is accounted for
        with a function of temperature (_H_f_T) """
        uncomp = self._Sh_2_H(self._cmds[self.HTU21D_HOLD_TRIG_HUMI_MEAS] >> 8)
        return uncomp + self._H_f_T()

    @humidity.setter
    def humidity(self, new_H:float):
        uncomp = float(new_H) - self._H_f_T()
        new_Sh = self._H_2_Sh(uncomp)
        self._cmds[self.HTU21D_HOLD_TRIG_HUMI_MEAS] = (new_Sh << 8) | self._crc(new_Sh)




def main():
    htu21d = i2c_device_htu21d_t()
    print(f"temp = {htu21d.temperature}")
    print(f"humi = {htu21d.humidity   }")
    print(f"humi_raw = {hex(htu21d._cmds[0xE5])}")
    htu21d.temperature = 25.
    print(f"temp = {htu21d.temperature}")
    print(f"humi = {htu21d.humidity   }")
    print(f"humi_raw = {hex(htu21d._cmds[0xE5])}")
    htu21d.humidity = 30.
    print(f"temp = {htu21d.temperature}")
    print(f"humi = {htu21d.humidity   }")
    print(f"humi_raw = {hex(htu21d._cmds[0xE5])}")
    htu21d.temperature = 15.
    print(f"temp = {htu21d.temperature}")
    print(f"humi = {htu21d.humidity   }")
    print(f"humi_raw = {hex(htu21d._cmds[0xE5])}")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
