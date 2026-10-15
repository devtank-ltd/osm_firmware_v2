#pragma once

void osm_adc_loop_init(void);

void osm_adc_loop_channel_add(unsigned channel, uint16_t adc);

void osm_adc_loop_interate(void);
