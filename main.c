#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>


static void
task1(void *args __attribute((unused))) {

	for (;;) {
		gpio_toggle(GPIOA, GPIO5);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_48mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);

    for(unsigned n = 0; n < 10; n++)
    {
        for(unsigned n = 0; n < 10000000; n++)
            asm("nop");
        gpio_toggle(GPIOA, GPIO5);
    }

    xTaskCreate(task1,"LED",100,NULL,configMAX_PRIORITIES-1,NULL);
    vTaskStartScheduler();

    return 0;
}

