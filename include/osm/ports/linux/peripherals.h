#pragma once

void osm_peripherals_add_modbus(unsigned uart, unsigned* pid);
void osm_peripherals_add_hpm(unsigned uart, unsigned* pid);
void osm_peripherals_add_cmd(unsigned* pid);
void osm_peripherals_add_example_rs232(unsigned uart, unsigned* pid);

void osm_peripherals_add_w1(unsigned timeout_us, unsigned* pid);
void osm_peripherals_add_i2c(unsigned timeout_us, unsigned* pid);

bool osm_peripherals_add(const char * app_rel_path, const char * ready_path, unsigned timeout_us, unsigned* pid);

void osm_peripherals_close(unsigned* pids, unsigned count);
