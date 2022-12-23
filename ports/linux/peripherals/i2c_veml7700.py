#!/usr/bin/env python3

import numpy as np
from scipy.interpolate import interp1d

from basetypes import i2c_device_t


VEML7700_DEFAULT_LUX = 1000


class i2c_device_veml7700_t(i2c_device_t):
    VEML7700_ADDR   = 0x10

    VEML7700_ALS_CONF_0         = 0x00
    VEML7700_ALS_WH             = 0x01
    VEML7700_ALS_WL             = 0x02
    VEML7700_ALS_POWER_SAVING   = 0x03
    VEML7700_ALS                = 0x04
    VEML7700_ALS_WHITE          = 0x05
    VEML7700_ALS_INT            = 0x06

    VEML7700_CMDS   = {VEML7700_ALS_CONF_0      : 0x0001,
                       VEML7700_ALS_WH          : 0x0000,
                       VEML7700_ALS_WL          : 0x0000,
                       VEML7700_ALS_POWER_SAVING: 0x0000,
                       VEML7700_ALS             : 0x0000,
                       VEML7700_ALS_WHITE       : 0x0000,
                       VEML7700_ALS_INT         : 0x0000}

    VEML7700_COEFFS = [+3.7649e-12,
                       -5.8803e-08,
                       +5.1017e-04,
                       +6.2751e+00]

    _ALS_GAIN_MASK  = 0b11
    _ALS_GAIN_SHIFT = 11
    _ALS_GAIN_X1    = 0b00
    _ALS_GAIN_X2    = 0b01
    _ALS_GAIN_X1_8  = 0b10
    _ALS_GAIN_X1_4  = 0b11
    _ALS_GAIN_MODES         = { _ALS_GAIN_X1   : 1.   ,
                                _ALS_GAIN_X2   : 2.   ,
                                _ALS_GAIN_X1_8 : 1./8.,
                                _ALS_GAIN_X1_4 : 1./4.}
    _ALS_GAIN_MULTIPLIERS   = { _ALS_GAIN_X1   : 2    ,
                                _ALS_GAIN_X2   : 1    ,
                                _ALS_GAIN_X1_8 : 16   ,
                                _ALS_GAIN_X1_4 : 8    }

    _ALS_IT_MASK   = 0b1111
    _ALS_IT_SHIFT  = 6
    _ALS_IT_25_MS  = 0b1100
    _ALS_IT_50_MS  = 0b1000
    _ALS_IT_100_MS = 0b0000
    _ALS_IT_200_MS = 0b0001
    _ALS_IT_400_MS = 0b0010
    _ALS_IT_800_MS = 0b0011

    _ALS_IT_MODES       = {_ALS_IT_25_MS  : 25 ,
                           _ALS_IT_50_MS  : 50 ,
                           _ALS_IT_100_MS : 100,
                           _ALS_IT_200_MS : 200,
                           _ALS_IT_400_MS : 400,
                           _ALS_IT_800_MS : 800}
    _ALS_IT_MULTIPLIERS = {_ALS_IT_25_MS  : 32,
                           _ALS_IT_50_MS  : 16,
                           _ALS_IT_100_MS :  8,
                           _ALS_IT_200_MS :  4,
                           _ALS_IT_400_MS :  2,
                           _ALS_IT_800_MS :  1}

    def __init__(self, lux=VEML7700_DEFAULT_LUX):
        super().__init__(self.VEML7700_ADDR, self.VEML7700_CMDS)
        self._inv_dt_correction = lambda x: 1
        self._get_inverse_correction()

        self.lux = lux

    def transfer(self, data):
        r = super().transfer(data)
        self.lux = self._lux
        return r

    @property
    def resolution(self):
        conf = self._cmds[self.VEML7700_ALS_CONF_0]
        gain_mode = (conf >> self._ALS_GAIN_SHIFT) & self._ALS_GAIN_MASK
        gain_multipler = self._ALS_GAIN_MULTIPLIERS[gain_mode]

        integration_mode = (conf >> self._ALS_IT_SHIFT) & self._ALS_IT_MASK
        integration_multiplier = self._ALS_IT_MULTIPLIERS[integration_mode]

        return 0.0036 * gain_multipler * integration_multiplier

    def _dt_correction(self, lux):
        return (self.VEML7700_COEFFS[0] * lux**4 +
                self.VEML7700_COEFFS[1] * lux**3 +
                self.VEML7700_COEFFS[2] * lux**2 +
                self.VEML7700_COEFFS[3] * lux )

    def _get_inverse_correction(self):
        h_eval = np.linspace(0, 2500, 50)
        self._inv_dt_correction = interp1d(self._dt_correction(h_eval), h_eval, kind="cubic", bounds_error=True)

    @resolution.setter
    def resolution(self, new_res):
        raise NotImplemented("Not Implemented")

    def _from_counts(self, counts):
        lux = counts * self.resolution
        return self._dt_correction(lux)

    def _to_counts(self, lux_corrected):
        lux = self._inv_dt_correction(lux_corrected)
        return lux / self.resolution

    def _decode(self, mem):
        return ((int(mem) & 0x00FF) << 8) | ((int(mem) & 0xFF00) >> 8)

    def _encode(self, counts):
        return self._decode(counts)

    @property
    def lux(self):
        counts = self._decode(self._cmds[self.VEML7700_ALS])
        return self._from_counts(counts)

    @lux.setter
    def lux(self, new_lux:float):
        self._lux = new_lux
        counts = self._to_counts(new_lux)
        self._cmds[self.VEML7700_ALS] = self._encode(counts)


def main():
    veml7700 = i2c_device_veml7700_t()
    print(veml7700.lux)
    veml7700.lux = 2300
    print(veml7700.lux)
    veml7700.lux = 0.1
    print(veml7700.lux)
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
