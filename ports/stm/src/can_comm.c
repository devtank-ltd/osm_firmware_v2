#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "can_comm.h"
#include "pinmap.h"
#include "common.h"
#include "log.h"


#define CAN_COMM_BUFFER_NUM                                         10


typedef struct
{
    uint32_t        unit;
    uint32_t        rcc;
    port_n_pins_t   stdby;
    port_n_pins_t   rx;
    uint8_t         rx_af;
    port_n_pins_t   tx;
    uint8_t         tx_af;
} can_comm_config_t;


static can_comm_config_t _can_comm_config                       = CAN_CONFIG;


static can_comm_data_t  _can_comm_data_arr[CAN_COMM_BUFFER_NUM] = {0};
ring_buf_t              can_comm_ring_data                      = RING_BUF_INIT((volatile char *)_can_comm_data_arr, sizeof(can_comm_data_t));
static bool             _can_comm_enabled                       = false;


static void _can_comm_clk_init(void)
{
    rcc_periph_clock_enable(CAN_RCC_TIM);

    timer_disable_counter(CAN_TIM);

    timer_set_mode(CAN_TIM,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);

    timer_set_prescaler(CAN_TIM, rcc_ahb_frequency / 10000-1);//-1 because it starts at zero, and interrupts on the overflow
    timer_set_period(CAN_TIM, 5);
    timer_enable_preload(CAN_TIM);
    timer_continuous_mode(CAN_TIM);
    timer_enable_irq(CAN_TIM, TIM_DIER_CC1IE);
}


void can_comm_init(void)
{
    port_n_pins_t* can_stdby = &_can_comm_config.stdby;
    port_n_pins_t* can_rx    = &_can_comm_config.rx;
    port_n_pins_t* can_tx    = &_can_comm_config.tx;
    rcc_periph_clock_enable(PORT_TO_RCC(can_stdby->port));
    rcc_periph_clock_enable(PORT_TO_RCC(can_rx->port));
    rcc_periph_clock_enable(PORT_TO_RCC(can_tx->port));
    gpio_mode_setup(can_rx->port, GPIO_MODE_AF, GPIO_PUPD_NONE, can_rx->pins);
    gpio_mode_setup(can_tx->port, GPIO_MODE_AF, GPIO_PUPD_NONE, can_tx->pins);
    gpio_mode_setup(can_stdby->port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, can_stdby->pins);

    // Enable clock to the CAN peripheral
    rcc_periph_clock_enable(_can_comm_config.rcc);

    gpio_set_af(can_rx->port, _can_comm_config.rx_af, can_rx->pins);
    gpio_set_af(can_tx->port, _can_comm_config.tx_af, can_tx->pins);

    // Reset the can peripheral
    can_reset(_can_comm_config.unit);

    // Initialize the can peripheral
    can_init(
            _can_comm_config.unit, // The can ID

            // Time Triggered Communication Mode?
            // http://www.datamicro.ru/download/iCC_07_CANNetwork_with_Time_Trig_Communication.pdf
            false, // No TTCM

            // Automatic bus-off management?
            // When the bus error counter hits 255, the CAN will automatically
            // remove itself from the bus. if ABOM is disabled, it won't
            // reconnect unless told to. If ABOM is enabled, it will recover the
            // bus after the recovery sequence.
            true, // Yes ABOM

            // Automatic wakeup mode?
            // 0: The Sleep mode is left on software request by clearing the SLEEP
            // bit of the CAN_MCR register.
            // 1: The Sleep mode is left automatically by hardware on CAN
            // message detection.
            true, // Do not wake up on message rx

            // No automatic retransmit?
            // If true, will not automatically attempt to re-transmit messages on
            // error
            true, // Do not auto-retry

            // Receive FIFO locked mode?
            // If the FIFO is in locked mode,
            //  once the FIFO is full NEW messages are discarded
            // If the FIFO is NOT in locked mode,
            //  once the FIFO is full OLD messages are discarded
            false, // Discard older messages over newer

            // Transmit FIFO priority?
            // This bit controls the transmission order when several mailboxes are
            // pending at the same time.
            // 0: Priority driven by the identifier of the message
            // 1: Priority driven by the request order (chronologically)
            false, // TX priority based on identifier

            //// Bit timing settings
            //// Assuming 80MHz base clock, 87.5% sample point, 62.5 kBit/s data rate
            //// http://www.bittiming.can-wiki.info/
            // Resync time quanta jump width
            CAN_BTR_SJW_1TQ, // 16,
            // Time segment 1 time quanta width
            CAN_BTR_TS1_11TQ, // 13,
            // Time segment 2 time quanta width
            CAN_BTR_TS2_4TQ, // 2,
            // Baudrate prescaler
            80,

            // Loopback mode
            // If set, CAN can transmit but not receive
            false,

            // Silent mode
            // If set, CAN can receive but not transmit
            false);

    gpio_clear(can_stdby->port, can_stdby->pins);

    /*
     Must use a timer as STM32L451 does not support CAN interrupts, UGH!
    */
    _can_comm_clk_init();
    if (_can_comm_enabled)
        timer_enable_counter(CAN_TIM);

    nvic_enable_irq(NVIC_TIM3_IRQ);
}


static bool _can_comm_new_data(can_comm_packet_t* pkt)
{
    // Need to check if length is size of packet or the size of data.
    if (!ring_buf_add_data(&can_comm_ring_data, &pkt->header, sizeof(can_comm_header_t)))
        return false;
    return ring_buf_add_data(&can_comm_ring_data, pkt->data, pkt->header.length);
}


void tim3_isr(void)
{
    timer_clear_flag(CAN_TIM, TIM_SR_CC1IF);

    can_comm_packet_t pkt;
    uint8_t data[CAN_COMM_MAX_DATA_SIZE];
    pkt.data = data;

    can_receive(_can_comm_config.unit, 0, false, &pkt.header.id, &pkt.header.ext, &pkt.header.rtr, &pkt.header.fmi, &pkt.header.length, pkt.data, NULL);

    can_fifo_release(_can_comm_config.unit, 0);

    if (pkt.header.length > CAN_COMM_MAX_DATA_SIZE)
        pkt.header.length = CAN_COMM_MAX_DATA_SIZE;

    // Unused bool func
    _can_comm_new_data(&pkt);
    if (!_can_comm_enabled)
        timer_disable_counter(CAN_TIM);
}


void can_comm_enable(bool enabled)
{
    _can_comm_enabled = enabled;
    if (enabled)
        timer_enable_counter(CAN_TIM);
}


void can_comm_send(can_comm_packet_t* pkt)
{
    can_debug("Sending %"PRIu32" len:%u ext:%"PRIu8" rtr:%"PRIu8, pkt->header.id, pkt->header.length, (uint8_t)pkt->header.ext, (uint8_t)pkt->header.rtr);
    log_debug_data(DEBUG_CAN, pkt->data, pkt->header.length);
    can_transmit(_can_comm_config.unit, pkt->header.id, pkt->header.ext, pkt->header.rtr, pkt->header.length, pkt->data);
}
