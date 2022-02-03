#include <ctype.h>
#include <stdlib.h>

#include "base_types.h"
#include "log.h"


char * skip_space(char * pos)
{
    while(*pos == ' ')
        pos++;
    return pos;
}


bool decompose_uart_str(char             * str,
                        uint32_t         * speed,
                        uint8_t          * databits,
                        uart_parity_t    * parity,
                        uart_stop_bits_t * stop)
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
