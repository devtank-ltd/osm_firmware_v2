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
    "python",
]

##
# @brief Import all variables in these modules into other namespaces
#
from .python import *
