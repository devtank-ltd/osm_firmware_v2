#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "pinmap.h"
#include "io.h"
#include "log.h"
#include "pulsecount.h"
#include "uarts.h"
#include "platform.h"
#include "w1.h"
#include "persist_config.h"
#include "common.h"
#include "platform_model.h"

static const port_n_pins_t ios_pins[]           = IOS_PORT_N_PINS;
static uint16_t * ios_state;

#define IOS_SPECIAL_STR_LEN                         32
#define IOS_SPECIAL_INDIV_STR_LEN                   10


static bool _ios_append_special_str(char special_str[IOS_SPECIAL_STR_LEN+1], const char* new_str)
{
    uint16_t space = IOS_SPECIAL_STR_LEN - strnlen(special_str, IOS_SPECIAL_STR_LEN);
    if (space < strnlen(new_str, IOS_SPECIAL_INDIV_STR_LEN))
    {
        io_debug("Out of space for line.");
        return false;
    }
    strncat(special_str, new_str, space);
    return true;
}


static char* _ios_get_type_active(uint16_t io_state)
{
    switch(io_state & IO_ACTIVE_SPECIAL_MASK)
    {
        case IO_SPECIAL_PULSECOUNT_RISING_EDGE:
            return "PLSCNT R";
        case IO_SPECIAL_PULSECOUNT_FALLING_EDGE:
            return "PLSCNT F";
        case IO_SPECIAL_PULSECOUNT_BOTH_EDGE:
            return "PLSCNT B";
        case IO_SPECIAL_ONEWIRE:
            return "W1";
        default:
            return "";
    }
}


static char* _ios_get_type_possible(unsigned io)
{
    static char special_str[IOS_SPECIAL_STR_LEN+1];
    memset(special_str, 0, IOS_SPECIAL_STR_LEN+1);
    unsigned count = 0;
    for (unsigned n = 1; n < IO_SPECIAL_MAX; n++)
    {
        if (!model_can_io_be_special(io, n))
            continue;
        int16_t space = IOS_SPECIAL_STR_LEN - strnlen(special_str, IOS_SPECIAL_STR_LEN);
        if (space < 3)
        {
            io_debug("Out of space for line.");
            return special_str;
        }
        if (count)
        {
            if (!_ios_append_special_str(special_str, " | "))
                return special_str;
        }
        count++;
        space = IOS_SPECIAL_STR_LEN - strnlen(special_str, IOS_SPECIAL_STR_LEN);
        switch(n)
        {
            case IO_SPECIAL_PULSECOUNT_RISING_EDGE:
            case IO_SPECIAL_PULSECOUNT_FALLING_EDGE:
            case IO_SPECIAL_PULSECOUNT_BOTH_EDGE:
            {
                if (!_ios_append_special_str(special_str, "PLSCNT"))
                    return special_str;
                break;
            }
            case IO_SPECIAL_ONEWIRE:
            {
                if (!_ios_append_special_str(special_str, "W1"))
                    return special_str;
                break;
            }
            default:
            {
                if (!_ios_append_special_str(special_str, "UNKWN"))
                    return special_str;
                break;
            }
        }
    }
    return special_str;
}


bool ios_get_pupd(unsigned io, uint8_t* pupd)
{
    if (!pupd)
    {
        io_debug("Handed NULL pointer.");
        return false;
    }
    if (io >= IOS_COUNT)
    {
        io_debug("IO '%u' out of range.", io);
        return false;
    }
    *pupd = ios_state[io] & IO_PULL_MASK;
    return true;
}


static void _ios_setup_gpio(unsigned io, uint16_t io_state)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    char * type = _ios_get_type_possible(io);

    const port_n_pins_t * gpio_pin = &ios_pins[io];

    platform_gpio_setup(gpio_pin, io_state & IO_AS_INPUT, io_state & IO_PULL_MASK);

    ios_state[io] = (ios_state[io] & (IO_ACTIVE_SPECIAL_MASK)) | io_state;

    io_debug("%02u set to %s %s%s%s%s",
            io,
            (io_state & IO_AS_INPUT)?"IN":"OUT",
            io_get_pull_str(io_state),
            (type[0])?" [":"",type,(type[0])?"]":"");
}


void     ios_init(void)
{
    ios_state = persist_data.model_config.ios_state;

    if (ios_state[0] == 0 || ios_state[0] == 0xFFFF)
    {
        io_debug("No IOs state, setting to default.");
        uint16_t ios_init_state[ARRAY_SIZE(ios_pins)] = IOS_STATE;
        memcpy(ios_state, ios_init_state, sizeof(ios_init_state));
    }

    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
    {
        platform_gpio_init(&ios_pins[n]);

        uint16_t io_state = ios_state[n];

        if (io_state & IO_ACTIVE_SPECIAL_MASK)
        {
            if (io_state & IO_SPECIAL_ONEWIRE)
                w1_enable(n, true);
            if (io_state & IO_SPECIAL_PULSECOUNT_RISING_EDGE    ||
                io_state & IO_SPECIAL_PULSECOUNT_FALLING_EDGE   ||
                io_state & IO_SPECIAL_PULSECOUNT_BOTH_EDGE      )
            {
                pulsecount_enable(n, true, io_state & IO_PULL_MASK, io_state & IO_ACTIVE_SPECIAL_MASK);
            }
            io_debug("%02u : USED %s", n, _ios_get_type_active(io_state));
        }
        else
            _ios_setup_gpio(n, io_state);
    }
}


unsigned ios_get_count(void)
{
    return ARRAY_SIZE(ios_pins);
}


bool io_enable_w1(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    if (!model_can_io_be_special(io, IO_SPECIAL_ONEWIRE))
        return false;

    ios_state[io] &= ~IO_OUT_ON;
    ios_state[io] &= ~IO_AS_INPUT;

    ios_state[io] &= ~IO_ACTIVE_SPECIAL_MASK;
    ios_state[io] |= IO_SPECIAL_ONEWIRE;
    pulsecount_enable(io, false, IO_PUPD_NONE, IO_SPECIAL_NONE);
    w1_enable(io, true);
    io_debug("%02u : USED W1", io);
    return true;
}


static unsigned io_pull(io_pupd_t pull)
{
    switch(pull)
    {
        case IO_PUPD_NONE:
            return GPIO_PUPD_NONE;
        case IO_PUPD_UP:
            return GPIO_PUPD_PULLUP;
        case IO_PUPD_DOWN:
            return GPIO_PUPD_PULLDOWN;
    }
    return GPIO_PUPD_NONE;
}


bool io_enable_pulsecount(unsigned io, io_pupd_t pupd, io_special_t edge)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    if (!model_can_io_be_special(io, edge))
        return false;

    ios_state[io] &= ~IO_OUT_ON;
    ios_state[io] &= ~IO_AS_INPUT;

    ios_state[io] &= ~IO_PULL_MASK;
    ios_state[io] |= (io_pull(pupd) & IO_PULL_MASK);

    ios_state[io] &= ~IO_ACTIVE_SPECIAL_MASK;
    ios_state[io] |= edge;

    w1_enable(io, false);
    pulsecount_enable(io, true, pupd, edge);
    io_debug("%02u : USED PLSCNT", io);
    return true;
}


void     io_configure(unsigned io, bool as_input, io_pupd_t pull)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    uint16_t io_state = ios_state[io];

    if ((io_state & IO_ACTIVE_SPECIAL_MASK))
    {
        ios_state[io] &= ~IO_ACTIVE_SPECIAL_MASK;
        w1_enable(io, false);
        pulsecount_enable(io, false, IO_PUPD_NONE, IO_SPECIAL_NONE);
        io_debug("%02u : NO LONGER SPECIAL", io);
        return;
    }

    if (io_state & IO_DIR_LOCKED)
    {
        io_debug("GPIO %02u locked but change attempted.", io);
        return;
    }

    io_state &= ~IO_OUT_ON;

    if (as_input)
        io_state |= IO_AS_INPUT;
    else
        io_state &= ~IO_AS_INPUT;

    io_state &= ~IO_PULL_MASK;
    io_state |= (io_pull(pull) & IO_PULL_MASK);

    _ios_setup_gpio(io, io_state);
}


bool io_is_input(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return ios_state[io] & IO_AS_INPUT;
}


bool io_is_w1_now(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return (ios_state[io] & IO_ACTIVE_SPECIAL_MASK) == IO_SPECIAL_ONEWIRE;
}


bool io_is_pulsecount_now(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    uint16_t special = ios_state[io] & IO_ACTIVE_SPECIAL_MASK;
    return (special == IO_SPECIAL_PULSECOUNT_RISING_EDGE    ||
            special == IO_SPECIAL_PULSECOUNT_FALLING_EDGE   ||
            special == IO_SPECIAL_PULSECOUNT_BOTH_EDGE      );
}


unsigned io_get_bias(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return false;

    return (ios_state[io] & IO_PULL_MASK);
}


void     io_on(unsigned io, bool on_off)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * output = &ios_pins[io];

    if (ios_state[io] & IO_AS_INPUT)
        return;

    io_debug("%02u = %s", io, (on_off)?"ON":"OFF");

    if (on_off)
    {
        ios_state[io] |= IO_OUT_ON;
        platform_gpio_set(output, true);
    }
    else
    {
        ios_state[io] &= ~IO_OUT_ON;
        platform_gpio_set(output, false);
    }
}


void     io_log(unsigned io)
{
    if (io >= ARRAY_SIZE(ios_pins))
        return;

    const port_n_pins_t * gpio_pin = &ios_pins[io];
    uint16_t io_state = ios_state[io];

    if (!(io_state & IO_ACTIVE_SPECIAL_MASK))
    {
        char * pretype = "";
        char * posttype = "";

        char * type = _ios_get_type_possible(io);

        if (type[0])
        {
            pretype = "[";
            posttype = "] ";
        }
        else
        {
            pretype = "";
            posttype = " ";
        }

        if (io_state & IO_AS_INPUT)
            log_out("IO %02u : %s%s%sIN %s = %s",
                    io, pretype, type, posttype,
                    io_get_pull_str(io_state),
                    (platform_gpio_get(gpio_pin))?"ON":"OFF");
        else
            log_out("IO %02u : %s%s%sOUT %s = %s",
                    io, pretype, type, posttype,
                    io_get_pull_str(io_state),
                    (platform_gpio_get(gpio_pin))?"ON":"OFF");
    }
    else
    {
        char * type = _ios_get_type_active(io_state);
        char pupd_char;
        if ((io_state & IO_PULL_MASK) == GPIO_PUPD_PULLUP)
            pupd_char = 'U';
        else if ((io_state & IO_PULL_MASK) == GPIO_PUPD_PULLDOWN)
            pupd_char = 'D';
        else if ((io_state & IO_PULL_MASK) == GPIO_PUPD_NONE)
            pupd_char = 'N';
        else
            pupd_char = ' ';
        log_out("IO %02u : USED %s %c", io, type, pupd_char);
    }
}


void     ios_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(ios_pins); n++)
        io_log(n);
}


static command_response_t _io_log_cb(char* args)
{
    ios_log();
    return COMMAND_RESP_OK;
}


static command_response_t _io_cb(char *args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);
    pos = skip_space(pos);
    bool do_read = false;

    if (*pos == ':')
    {
        do_read = true;
        bool as_input;
        pos = skip_space(pos + 1);
        if (strncmp(pos, "IN", 2) == 0 || *pos == 'I')
        {
            pos = skip_to_space(pos);
            as_input = true;
        }
        else if (strncmp(pos, "OUT", 3) == 0 || *pos == 'O')
        {
            pos = skip_to_space(pos);
            as_input = false;
        }
        else
        {
            log_error("Malformed gpio type command");
            return COMMAND_RESP_ERR;
        }

        io_pupd_t pull = IO_PUPD_NONE;

        pos = skip_space(pos);

        if (*pos && *pos != '=')
        {
            if ((strncmp(pos, "UP", 2) == 0) || (pos[0] == 'U'))
            {
                pos = skip_to_space(pos);
                pull = IO_PUPD_UP;
            }
            else if (strncmp(pos, "DOWN", 4) == 0 || pos[0] == 'D')
            {
                pos = skip_to_space(pos);
                pull = IO_PUPD_DOWN;
            }
            else if (strncmp(pos, "NONE", 4) == 0 || pos[0] == 'N')
            {
                pos = skip_to_space(pos);
            }
            else
            {
                log_error("Malformed gpio pull command");
                return COMMAND_RESP_ERR;
            }
            pos = skip_space(pos);
        }

        io_configure(io, as_input, pull);
    }

    if (*pos == '=')
    {
        pos = skip_space(pos + 1);
        if (strncmp(pos, "ON", 2) == 0 || *pos == '1')
        {
            pos = skip_to_space(pos);
            if (!io_is_input(io))
            {
                io_on(io, true);
                if (!do_read)
                    log_out("IO %02u = ON", io);
            }
            else log_error("IO %02u is input but output command.", io);
        }
        else if (strncmp(pos, "OFF", 3) == 0 || *pos == '0')
        {
            pos = skip_to_space(pos);
            if (!io_is_input(io))
            {
                io_on(io, false);
                if (!do_read)
                    log_out("IO %02u = OFF", io);
            }
            else log_error("IO %02u is input but output command.", io);
        }
        else
        {
            log_error("Malformed gpio on/off command");
            return COMMAND_RESP_ERR;
        }
    }
    else do_read = true;

    if (do_read)
        io_log(io);
    return COMMAND_RESP_OK;
}


static command_response_t _io_cmd_enable_pulsecount_cb(char * args)
{
    /* <io> <R/F/B> <U/D/N>
     */
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);
    if (args == pos)
        goto bad_exit;
    pos = skip_space(pos);
    io_special_t edge;
    if (pos[0] == 'R')
        edge = IO_SPECIAL_PULSECOUNT_RISING_EDGE;
    else if (pos[0] == 'F')
        edge = IO_SPECIAL_PULSECOUNT_FALLING_EDGE;
    else if (pos[0] == 'B')
        edge = IO_SPECIAL_PULSECOUNT_BOTH_EDGE;
    else
        goto bad_exit;

    pos = skip_space(pos+1);
    uint8_t pupd;
    if (pos[0] == 'U')
        pupd = IO_PUPD_UP;
    else if (pos[0] == 'D')
        pupd = IO_PUPD_DOWN;
    else if (pos[0] == 'N')
        pupd = IO_PUPD_NONE;
    else
        goto bad_exit;

    if (io_enable_pulsecount(io, pupd, edge))
        log_out("IO %02u pulsecount enabled", io);
    else
        log_out("IO %02u has no pulsecount", io);
    return COMMAND_RESP_OK;
bad_exit:
    log_out("<io> <R/F/B> <U/D/N>");
    return COMMAND_RESP_ERR;
}


static command_response_t _io_cmd_enable_onewire_cb(char * args)
{
    char * pos = NULL;
    unsigned io = strtoul(args, &pos, 10);

    if (io_enable_w1(io))
    {
        log_out("IO %02u onewire enabled", io);
        return COMMAND_RESP_OK;
    }

    log_out("IO %02u has no onewire", io);
    return COMMAND_RESP_ERR;
}


struct cmd_link_t* ios_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "ios",          "Print all IOs.",           _io_log_cb                        , false , NULL },
                                       { "io",           "Get/set IO set.",          _io_cb                            , false , NULL },
                                       { "en_pulse",     "Enable Pulsecount IO.",    _io_cmd_enable_pulsecount_cb      , false , NULL },
                                       { "en_w1",        "Enable OneWire IO.",       _io_cmd_enable_onewire_cb         , false , NULL }};
    tail = add_commands(tail, cmds, ARRAY_SIZE(cmds));
    return pulsecount_add_commands(tail);
}
