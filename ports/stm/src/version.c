#include "version.h"

#include "log.h"
#include "common.h"


#define FLASH_SIZE_STM32L452RE          256
#define FLASH_SIZE_STM32L433VTC6        64


static version_arch_t _version_arch = VERSION_ARCH_UNDEFINED;


void version_arch_error_handler(void)
{
    while(1);
}


static bool _version_get_arch(version_arch_t* arch)
{
   uint16_t flash_size = *((uint32_t*)DESIG_FLASH_SIZE_BASE);
   switch(flash_size)
   {
       case FLASH_SIZE_STM32L452RE:
           *arch = VERSION_ARCH_REV_B;
           return true;
       case FLASH_SIZE_STM32L433VTC6:
           *arch = VERSION_ARCH_REV_C;
           return true;
       default:
           break;
   }
   return false;
}


version_arch_t version_get_arch(void)
{
    if (_version_arch == VERSION_ARCH_UNDEFINED &&
        !_version_get_arch(&_version_arch))
        return VERSION_ARCH_UNDEFINED;
    return _version_arch;
}


bool version_is_arch(version_arch_t is_arch)
{
    return (version_get_arch() == is_arch);
}
