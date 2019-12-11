""" LoTI binding package"""
## @package loti_firmware
#
# LoTI firmware wrapper
#

import sys

VERSION = 1.0

if sys.version_info.major == 3:
    ##
    # @brief Publically exportable modules
    #
    __all__ = [
        "binding",
    ]
    
    ##
    # @brief Import all variables in these modules into other namespaces
    #
    from .binding import io_board_py_t
else:
    from binding import io_board_py_t
