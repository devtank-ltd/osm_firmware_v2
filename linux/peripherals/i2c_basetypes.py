class i2c_device_t(object):
    def __init__(self, addr, cmds):
        self.addr  = addr
        self._cmds = cmds
        self._last_cmd_code = None

    def _read(self, cmd_code):
        data_arr = []
        if cmd_code is None:
            cmd_code = self._last_cmd_code
        assert cmd_code is not None, "No command code given."
        orig_value = value_cpy = int(self._cmds[cmd_code])

        count = 0
        while value_cpy != 0:
            count += 1
            value_cpy = value_cpy >> 8

        for i in range(count, 0, -1):
            shift = 8 * (i - 1)
            this_value = orig_value >> shift
            data_arr.append(this_value & 0xFF)
        return data_arr

    def _write(self, cmd_code, w):
        assert isinstance(w, list), "w must be a list"
        len_w = len(w)
        assert len_w >= 1, f"w must have at least 1 element ({len_w})."
        vals = list(w)
        data = 0
        for i in range(0, len_w):
            data += (vals[i] & 0xFF) << (8 * i)
        self._cmds[cmd_code] = data
        return None

    def transfer(self, data):
        wn = data["wn"]
        w  = None
        rn = data["rn"]
        r  = None
        cmd_code = None
        if wn:
            assert wn == len(data["w"]), f"WN is not equal to length of W. ({wn} != {len(data['w'])})"
            self._last_cmd_code = cmd_code = data["w"][0]
            if wn > 1:
                w = self._write(cmd_code, data["w"][1:])
        if rn:
            r_raw = self._read(cmd_code)
            r_raw_len = len(r_raw)
            assert rn >= r_raw_len, f"RN less than the length of R. ({rn} != {r_raw_len})"
            r = [0 for x in range(0, rn-r_raw_len)] + r_raw
        return {"addr"   : self.addr,
                "wn"     : wn,
                "w"      : w,
                "rn"     : rn,
                "r"      : r}
