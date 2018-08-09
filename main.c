#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>


int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_48mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    while(1)
    {
        gpio_toggle(GPIOA, GPIO5);
        for(unsigned n = 0; n < 1000000; n++)
            asm("nop");
    }
    return 0;
}

