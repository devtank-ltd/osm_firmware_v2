#!/usr/bin/env python3


import os
import sys
import enum
import math
import yaml
import ctypes
import datetime
import subprocess


class value_64_t(ctypes.Structure):
    _fields_ = [
        ("sum", ctypes.c_longlong),
        ("max", ctypes.c_longlong),
        ("min", ctypes.c_longlong),
    ]

class value_f_t(ctypes.Structure):
    _fields_ = [
        ("sum", ctypes.c_int),
        ("max", ctypes.c_int),
        ("min", ctypes.c_int),
    ]

class value_s_t(ctypes.Structure):
    _fields_ = [
        ("str", ctypes.c_char * 23),
    ]


class measurements_value_t(ctypes.Union):
    """
    union
    {
        struct
        {
            int64_t sum;
            int64_t max;
            int64_t min;
        } value_64;
        struct
        {
            int32_t sum;
            int32_t max;
            int32_t min;
        } value_f;
        struct
        {
            char    str[MEASUREMENTS_VALUE_STR_LEN];
        } value_s;
    };
    """

    _fields_ = [
        ("value_64" , value_64_t  ),
        ("value_f"  , value_f_t   ),
        ("value_s"  , value_s_t   ),
    ]


class measurements_def_t(ctypes.Structure):
    """
    char     name[MEASURE_NAME_NULLED_LEN];             // Name of the measurement
    uint8_t  interval;                                  // System intervals happen every 5 mins, this is how many must pass for measurement to be sent
    uint8_t  samplecount;                               // Number of samples in the interval set. Must be greater than or equal to 1
    uint8_t  type:7;                                    // measurement_def_type_t
    uint8_t  is_immediate:1;                            // Should collect as soon to sending as possible.
    """
    _fields_ = [
        ("name"         , ctypes.c_char * 5 ),
        ("interval"     , ctypes.c_ubyte    ),
        ("samplecount"  , ctypes.c_ubyte    ),
        ("type"         , ctypes.c_ubyte, 7 ),
        ("is_immediate" , ctypes.c_ubyte, 1 ),
    ]

    """
    typedef enum
    {
        MODBUS        = 1,
        PM10          = 2,
        PM25          = 3,
        CURRENT_CLAMP = 4,
        W1_PROBE      = 5,
        HTU21D_HUM    = 6,
        HTU21D_TMP    = 7,
        BAT_MON       = 8,
        PULSE_COUNT   = 9,
        LIGHT         = 10,
        SOUND         = 11,
        FW_VERSION    = 12,
        FTMA          = 13,
        CUSTOM_0      = 14,
        CUSTOM_1      = 15,
        IO_READING    = 16,
        CONFIG_REVISION = 17,
        SEN5x = 18,
    } measurements_def_type_t;
    """
    class TYPES(enum.Enum):
        MODBUS          = 1
        PM10            = 2
        PM25            = 3
        CURRENT_CLAMP   = 4
        W1_PROBE        = 5
        HTU21D_HUM      = 6
        HTU21D_TMP      = 7
        BAT_MON         = 8
        PULSE_COUNT     = 9
        LIGHT           = 10
        SOUND           = 11
        FW_VERSION      = 12
        FTMA            = 13
        CUSTOM_0        = 14
        CUSTOM_1        = 15
        IO_READING      = 16
        CONFIG_REVISION = 17
        SEN5x           = 18


class measurements_data_t(ctypes.Structure):
    """
    measurements_value_t    value;
    uint8_t                 value_type:6;                                 /* measurements_value_type_t */
    uint8_t                 instant_send:1;
    uint8_t                 is_collecting:1;
    uint8_t                 num_samples:7;
    uint8_t                 has_sent:1;
    uint8_t                 num_samples_init:7;
    uint8_t                 _:1;
    uint8_t                 num_samples_collected:7;
    uint8_t                 __:1;
    uint32_t                collection_time_cache;
    """
    _fields_ = [
        ("value"                    , measurements_value_t      ),
        ("value_type"               , ctypes.c_ubyte        , 6 ),
        ("instant_send"             , ctypes.c_ubyte        , 1 ),
        ("is_collecting"            , ctypes.c_ubyte        , 1 ),
        ("num_samples"              , ctypes.c_ubyte        , 7 ),
        ("has_sent"                 , ctypes.c_ubyte        , 1 ),
        ("num_samples_init"         , ctypes.c_ubyte        , 7 ),
        ("_"                        , ctypes.c_ubyte        , 1 ),
        ("num_samples_collected"    , ctypes.c_ubyte        , 7 ),
        ("__"                       , ctypes.c_ubyte        , 1 ),
        ("collection_time_cache"    , ctypes.c_uint             ),
    ]

    """
    typedef enum
    {
        MEASUREMENTS_VALUE_TYPE_INVALID = 0,
        MEASUREMENTS_VALUE_TYPE_I64,
        MEASUREMENTS_VALUE_TYPE_STR,
        MEASUREMENTS_VALUE_TYPE_FLOAT,
    } measurements_value_type_t;
    """
    class VALUE_TYPE(enum.Enum):
        INVALID = 0,
        I64     = 1
        STR     = 2
        FLOAT   = 3


class test_blob(object):
    COLOUR_WHITE    = "\33[39m"
    COLOUR_GREY     = "\33[37m"
    COLOUR_GREEN    = "\33[32m"
    COLOUR_RED      = "\33[31m"
    COLOUR_DEFAULT  = COLOUR_WHITE

    COMMS_PROTOCOL_PATH = os.path.abspath(os.path.dirname(__file__) + "/debug.js")

    def __init__(self, lib_blob):
        self.lib = lib_blob
        self.lib.connect()
        self._TEST_COMMS_SEND_FUNC = ctypes.CFUNCTYPE(
            ctypes.c_bool,                  # Retval
            ctypes.POINTER(ctypes.c_byte),  # Param1
            ctypes.c_ushort)                # Param2
        self._test_comms_send_func = self._TEST_COMMS_SEND_FUNC(self._test_comms_send)
        self.lib.test_comms_send_set_cb(self._test_comms_send_func)

        self._LOG_ERROR_FUNC = ctypes.CFUNCTYPE(
            ctypes.c_void_p,                # Retval
            ctypes.c_char_p)                # Param1
        self._log_error_func = self._LOG_ERROR_FUNC(self._log_error)
        self.lib.log_error_set_cb(self._log_error_func)

        self._LOG_DEBUG_FUNC = ctypes.CFUNCTYPE(
            ctypes.c_void_p,                # Retval
            ctypes.c_uint,                  # Param1
            ctypes.c_char_p)                # Param2
        self._log_debug_func = self._LOG_DEBUG_FUNC(self._log_debug)
        self.lib.log_debug_set_cb(self._log_debug_func)

        self._sent_packet = None
        self.measurements = []

    def __del__(self):
        self.close()

    def close(self):
        pass

    def construct_measurement(self, name: str, avg: float, type_: measurements_def_t.TYPES, min_: float = None, max_: float = None, count: int = 5):
        if min_ is None:
            min_ = avg
        if max_ is None:
            max_ = avg
        sum_ = avg * count
        def_ = measurements_def_t(
            name            = name.encode(),
            interval        = 1,
            samplecount     = count,
            type            = type_.value,
            is_immediate    = 0)
        measurement_value = measurements_value_t(
            value_f = value_f_t(
                sum = int(sum_ * 1000),
                max = int(max_ * 1000),
                min = int(min_ * 1000)
                )
            )
        data = measurements_data_t(
            value                   = measurement_value,
            value_type              = measurements_data_t.VALUE_TYPE.FLOAT.value,
            instant_send            = 0,
            is_collecting           = 0,
            num_samples             = count,
            has_sent                = 0,
            num_samples_init        = count,
            _                       = 0,
            num_samples_collected   = count,
            __                      = 0,
            collection_time_cache   = 10)
        return (def_, data)

    def bool_check(self, name, condition, stop_on_fail=True):
        stat_str = "passed" if condition else "failed"
        colour = self.COLOUR_GREEN if condition else self.COLOUR_RED
        print(f"{colour}{name}: {stat_str}{self.COLOUR_DEFAULT}")
        if stop_on_fail and not condition:
            sys.exit(1)
        return condition

    def threshold_check(self, name, ref, val, threshold=0.05, stop_on_fail=False):
        condition = math.isclose(ref, val, abs_tol=threshold)
        stat_str = "passed" if condition else "failed"
        colour = self.COLOUR_GREEN if condition else self.COLOUR_RED
        comparitor = "==" if condition else "!="
        print(f"{colour}{name}: {ref} {comparitor} {val} (+/- {threshold}) {stat_str}{self.COLOUR_DEFAULT}")
        if stop_on_fail and not condition:
            sys.exit(1)
        return condition

    def _bool_check_add_measurement(self, *args, **kwargs):
        def_, data = self.construct_measurement(*args, **kwargs)
        self.measurements.append((def_, data))
        return self.bool_check(f"Append measurement '{args[0]}'",
            self.lib.protocol_append_measurement(ctypes.pointer(def_), ctypes.pointer(data))
            )

    def _threshold_check_measurement(self, dict_, measurement):
        def_, data = measurement
        name = def_.name.decode()
        success = self.bool_check(f"Decoding '{name}'",
            name in dict_.keys())
        avg = data.value.value_f.sum / (data.num_samples * 1000.)
        success &= self.threshold_check(f"Measurement '{name} Avg'", avg, dict_[name])
        if data.num_samples > 1:
            min_ = data.value.value_f.min / 1000.
            success &= self.threshold_check(f"Measurement '{name} Min'", min_, dict_[f"{name}_min"])
            max_ = data.value.value_f.max / 1000.
            success &= self.threshold_check(f"Measurement '{name} Min'", max_, dict_[f"{name}_max"])
        return success

    def test(self) -> bool:
        success = True
        self.lib.protocol_init()

        self.measurements = []
        success &= self._bool_check_add_measurement(
            "TEMP",
            23.5,
            measurements_def_t.TYPES.HTU21D_TMP,
            min_ = 23.,
            max_ = 24.)

        self._sent_packet = None
        success &= self.bool_check("Send protocol",
            self.lib.protocol_send()
            )
        if not self._sent_packet:
            self._log(f"Packet is empty? {self._sent_packet}")
            return False

        packet_str = "".join([f"{i:02X}" for i in self._sent_packet])

        command = f"js {self.COMMS_PROTOCOL_PATH}".split() + ["%s"%packet_str]
        self._log(command)
        with subprocess.Popen(command, stdout=subprocess.PIPE) as proc:
            resp = proc.stdout.read()
        resp_dict = yaml.safe_load(resp)
        self._log(resp_dict)
        for measurement in self.measurements:
            self._threshold_check_measurement(resp_dict, measurement)
        return success

    def _log(self, msg: str, **kwargs):
        print(f"{self.COLOUR_GREY}{self.now()}: {msg}{self.COLOUR_DEFAULT}", flush=True, **kwargs)

    def _log_error(self, msg: bytes):
        self._log(msg.decode(), file=sys.stderr)

    def _log_debug(self, flag: int, msg: bytes):
        self._log(msg.decode(), file=sys.stderr)

    def _test_comms_send(self, hex_arr: list, arr_len: int):
        self._sent_packet = [(256 + hex_arr[i]) % 256 for i in range(arr_len)] # Convert to uint8_t from int8_t
        self._log(f"SEND: {self._sent_packet}", file=sys.stderr)
        return True

    @staticmethod
    def now() -> str:
        return datetime.datetime.isoformat(datetime.datetime.utcnow())


def main(args):
    print("Hello World!");
    lib_blob = ctypes.CDLL(args[0])
    test_obj = test_blob(lib_blob)
    return 0 if test_obj.test() else 1;


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
