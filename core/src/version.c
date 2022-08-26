#include <libopencm3/stm32/iwdg.h>


#include "log.h"
#include "common.h"


#define FLASH_SIZE_STM32L452RE          256
#define FLASH_SIZE_STM32L433VTC6        64


#define FLASHING_TIME_SEC               2500


typedef enum
{
    TYPE_STM32L451RE,
    TYPE_STM32L433VTC6,
} type_arch_t;


static bool get_arch(uint8_t* arch)
{
    if (!arch)
        return false;
    uint16_t flash_size = *((uint32_t*)DESIG_FLASH_SIZE_BASE);
    switch(flash_size)
    {
        case FLASH_SIZE_STM32L452RE:
            log_out("STM is STM32L451RE");
            *arch = TYPE_STM32L451RE;
            return true;
        case FLASH_SIZE_STM32L433VTC6:
            log_out("STM is STM32L433VTC6");
            *arch = TYPE_STM32L433VTC6;
            return true;
        default:
            log_out("STM is unknown (%"PRIu16"kB flash)", flash_size);
            break;
    }
    return false;
}


static bool get_vers(uint8_t* vers)
{
#ifdef STM32L451RE
    *vers = TYPE_STM32L451RE;
#elif STM32L433VTC6
    *vers = TYPE_STM32L433VTC6;
#else
    return false;
#endif
    return true;
}


void check_version(void)
{
    uint8_t arch, vers;
    if (!get_arch(&arch) || !get_vers(&vers) || arch != vers)
    {
        uint32_t prev_now = 0;
        while(true)
        {
            iwdg_reset();
            while(since_boot_delta(get_since_boot_ms(), prev_now) < FLASHING_TIME_SEC);
            log_out("VERSION WRONG");
            prev_now = get_since_boot_ms();
        }
    }
}
