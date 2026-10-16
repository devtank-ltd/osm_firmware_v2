#pragma once

void osm_adc_loop_init(void);

void osm_adc_loop_channel_add(unsigned channel, uint16_t adc);

void osm_adc_loop_interate(void);

bool osm_adc_loop_set_interval(unsigned ms);

struct osm_cmd_link_t* osm_adc_loop_add(struct osm_cmd_link_t* tail);
