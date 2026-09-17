#pragma once
#include <cstddef>
#include <cstdint>
using i2c_master_dev_handle_t = void*;
using i2c_master_bus_handle_t = void*;
using esp_err_t = int;
constexpr int ESP_OK = 0;
constexpr int I2C_ADDR_BIT_LEN_7 = 0;
struct i2c_device_config_t {int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz;};
int i2c_master_bus_add_device(void*, const i2c_device_config_t*, void**);
int i2c_master_transmit_receive(void*, const void*, size_t, void*, size_t, int);
int i2c_master_transmit(void*, const void*, size_t, int);
