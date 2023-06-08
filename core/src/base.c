#include <ctype.h>
#include <stdlib.h>

#ifdef STM32L4
#include <libopencm3/stm32/gpio.h>
#endif //STM32L4

#include "base_types.h"
#include "log.h"


char * skip_space(char * pos)
{
    while(*pos == ' ')
        pos++;
    return pos;
}


char * skip_to_space(char * pos)
{
    while(*pos && *pos != ' ')
        pos++;
    return pos;
}


#define IO_PULL_STR_NONE "NONE"
#define IO_PULL_STR_UP   "UP"
#define IO_PULL_STR_DOWN "DOWN"


char* io_get_pull_str(uint16_t io_state)
{
    switch(io_state & IO_PULL_MASK)
    {
        case GPIO_PUPD_PULLUP:   return IO_PULL_STR_UP;
        case GPIO_PUPD_PULLDOWN: return IO_PULL_STR_DOWN;
        default:
            break;
    }
    return IO_PULL_STR_NONE;
}


bool io_is_special(uint16_t io_state)
{
    return (bool)(io_state & IO_ACTIVE_SPECIAL_MASK);
}


bool osm_decompose_uart_str(char             * str,
                        uint32_t         * speed,
                        uint8_t          * databits,
                        osm_uart_parity_t    * parity,
                        osm_uart_stop_bits_t * stop)
{
    if (!str || !speed || !databits || !parity || !stop)
        return false;
    
    if (!isdigit((unsigned char)*str))
    {
        log_error("Speed should be in decimal.");
        return false;
    }

    char * pos = NULL;
    *speed = strtoul(str, &pos, 10);
    pos = skip_space(++pos);
    if (!isdigit((unsigned char)*pos))
    {
        log_error("Bits should be given.");
        return false;
    }

    *databits = (uint8_t)(*pos) - (uint8_t)'0';
    pos = skip_space(++pos);

    switch(*pos)
    {
        case 'N' : *parity = uart_parity_none; break;
        case 'E' : *parity = uart_parity_even; break;
        case 'O' : *parity = uart_parity_odd; break;
        default:
        {
            log_error("Unknown parity type");
            return false;
        }
    }
    pos = skip_space(++pos);

    switch(*pos)
    {
        case '1' :
        {
            if (pos[1])
            {
                if (pos[1] != '.' || pos[2] != '5')
                {
                    log_error("Unknown stop bits count.");
                    return false;
                }
                else *stop = uart_stop_bits_1_5;
            }
            else *stop = uart_stop_bits_1;
            break;
        }
        case '2' : *stop = uart_stop_bits_2; break;
        default:
        {
            log_error("Unknown stop bits count.");
            return false;
        }
    }

    return true;
}
