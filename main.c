#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>


static void flash_led(unsigned delay) {
    for(unsigned n = 0; n < delay; n++)
        asm("nop");
    gpio_toggle(GPIOA, GPIO5);
}


static void
task1(void *args __attribute((unused))) {

	for (;;) {
		flash_led(1000000);
		vTaskDelay(5000000);
		flash_led(1000000);
	}
}

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_48mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);

    xTaskCreate(task1,"LED",100,NULL,configMAX_PRIORITIES-1,NULL);
    vTaskStartScheduler();

    return 0;
}

