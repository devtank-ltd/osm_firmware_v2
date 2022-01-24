""" OSM binding package"""
## @package OSM firmware
#
# OSM firmware wrapper
#

VERSION = 1.0

##
# @brief Publically exportable modules
#
__all__ = [
    "binding",
]

##
# @brief Import all variables in these modules into other namespaces
#
from .binding import debug_print as io_debug_print, set_debug_print as io_set_debug_print, get_debug_print as io_get_debug_print
from .binding import dev_t
