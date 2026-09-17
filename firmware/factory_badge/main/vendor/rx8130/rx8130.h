// SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <ctime>
#include <driver/i2c_master.h>

// Adapted from the factory RX8130 driver. Checked I/O, UTC, no calendar repair,
// no backup charging changes, and no mutation of the caller's struct tm.
class Rx8130 {
public:
    bool begin(i2c_master_bus_handle_t bus);
    bool readUtc(int64_t& epoch);
    bool writeUtc(int64_t epoch);
private:
    i2c_master_dev_handle_t device_ = nullptr;
    bool read(uint8_t reg, uint8_t* bytes, size_t count);
    bool write(uint8_t reg, const uint8_t* bytes, size_t count);
};
