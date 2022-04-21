import ctypes

_libsocketcan = ctypes.cdll.LoadLibrary("libsocketcan.so")

_as_byte_str = lambda s : s.encode() if isinstance(s, str) else s



def _CFUNCTYPE(name, argtypes):
    f = getattr(_libsocketcan, name)
    f.restype = ctypes.c_int
    f.argtypes = argtypes
    return lambda *args: f(_as_byte_str(args[0]), *args[1:]) \
           if len(argtypes) > 1 else \
           lambda *args: f(_as_byte_str(args[0]))


class can_berr_counter(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("txerr",  ctypes.c_uint16),
                ("rxerr",  ctypes.c_uint16)]


class can_device_stats(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("bus_error"       , ctypes.c_uint32),
                ("error_warning"   , ctypes.c_uint32),
                ("error_passive"   , ctypes.c_uint32),
                ("bus_off"         , ctypes.c_uint32),
                ("arbitration_lost", ctypes.c_uint32),
                ("restarts"        , ctypes.c_uint32)]


class can_ctrlmode(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("mask" , ctypes.c_uint32),
                ("flags", ctypes.c_uint32)]

    flag_has_loopback       = 0x01
    flag_has_listenonly     = 0x02
    flag_has_3_samples      = 0x04
    flag_has_one_shot       = 0x08
    flag_has_berr_reporting = 0x10
    flag_has_fd             = 0x20
    flag_has_presume_ack    = 0x40
    flag_has_fd_non_iso     = 0x80

    @property
    def valid_flags(self):
        return self.mask & self.flags



class rtnl_link_stats64(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("rx_packets"      , ctypes.c_uint64),
                ("tx_packets"      , ctypes.c_uint64),
                ("rx_bytes"        , ctypes.c_uint64),
                ("tx_bytes"        , ctypes.c_uint64),
                ("rx_errors"       , ctypes.c_uint64),
                ("tx_errors"       , ctypes.c_uint64),
                ("rx_dropped"      , ctypes.c_uint64),
                ("tx_dropped"      , ctypes.c_uint64),
                ("multicast"       , ctypes.c_uint64),
                ("collisions"      , ctypes.c_uint64),
                ("rx_length_errors", ctypes.c_uint64),
                ("rx_over_errors"  , ctypes.c_uint64),
                ("rx_crc_errors"   , ctypes.c_uint64),
                ("rx_frame_errors" , ctypes.c_uint64),
                ("rx_fifo_errors"  , ctypes.c_uint64),
                ("rx_missed_errors" , ctypes.c_uint64),
                ("tx_aborted_errors", ctypes.c_uint64),
                ("tx_carrier_errors", ctypes.c_uint64),
                ("tx_fifo_errors"     , ctypes.c_uint64),
                ("tx_heartbeat_errors", ctypes.c_uint64),
                ("tx_window_errors"   , ctypes.c_uint64),
                ("rx_compressed"      , ctypes.c_uint64),
                ("tx_compressed"      , ctypes.c_uint64),
                ("rx_nohandler"       , ctypes.c_uint64)]



can_state_map = {
0:"ACTIVE",
1:"WARNING",
2:"PASSIVE",
3:"BUS_OFF",
4:"STOPPED",
5:"SLEEPING"
}

can_do_restart = _CFUNCTYPE("can_do_restart", [ctypes.c_char_p])
can_do_stop    = _CFUNCTYPE("can_do_stop", [ctypes.c_char_p])
can_do_start   = _CFUNCTYPE("can_do_start", [ctypes.c_char_p])

can_set_restart_ms = _CFUNCTYPE("can_set_restart_ms", [ctypes.c_char_p, ctypes.c_uint32])
can_set_bitrate    = _CFUNCTYPE("can_set_bitrate", [ctypes.c_char_p, ctypes.c_uint32])
can_set_ctrlmode_raw   = _CFUNCTYPE("can_set_ctrlmode", [ctypes.c_char_p, ctypes.POINTER(can_ctrlmode)])

can_get_state_raw = _CFUNCTYPE("can_get_state", [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)])
can_get_ctrlmode_raw   = _CFUNCTYPE("can_get_ctrlmode", [ctypes.c_char_p, ctypes.POINTER(can_ctrlmode)])
can_get_berr_counter_raw = _CFUNCTYPE("can_get_berr_counter", [ctypes.c_char_p, ctypes.POINTER(can_berr_counter)])
can_get_device_stats_raw = _CFUNCTYPE("can_get_device_stats", [ctypes.c_char_p, ctypes.POINTER(can_device_stats)])
can_get_link_stats_raw   = _CFUNCTYPE("can_get_link_stats", [ctypes.c_char_p, ctypes.POINTER(rtnl_link_stats64)])

def can_get_state(name):
    if name.startswith("vcan"):
        return None
    v = ctypes.c_int(0)
    if not can_get_state_raw(name, ctypes.byref(v)):
        return can_state_map[v.value]

def can_set_ctrlmode(name, ctrlmode):
    if name.startswith("vcan"):
        return
    return can_set_ctrlmode_raw(name, ctypes.byref(ctrlmode))

def _basic_struct_get(f, name, struct_type):
    if name.startswith("vcan"):
        return None
    d = struct_type()
    if not f(name, ctypes.byref(d)):
        return d

can_get_ctrlmode     = lambda name : _basic_struct_get(can_get_ctrlmode_raw, name, can_ctrlmode)
can_get_berr_counter = lambda name : _basic_struct_get(can_get_berr_counter_raw, name, can_berr_counter)
can_get_device_stats = lambda name : _basic_struct_get(can_get_device_stats_raw, name, can_device_stats)
can_get_link_stats   = lambda name : _basic_struct_get(can_get_link_stats_raw, name, rtnl_link_stats64)
