#pragma once

void peripherals_add_modbus(unsigned uart);
void peripherals_add_hpm(unsigned uart);

void peripherals_add_w1(unsigned timeout_us);
void peripherals_add_i2c(unsigned timeout_us);

