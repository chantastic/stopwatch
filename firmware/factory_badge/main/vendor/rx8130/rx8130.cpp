// SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
// SPDX-License-Identifier: MIT
#include "rx8130.h"
#include <cstring>

namespace {
constexpr uint8_t Seconds = 0x10, Flags = 0x1d, Control = 0x1e;
constexpr uint8_t VoltageLow = 0x02, Stop = 0x40;
int fromBcd(uint8_t v) {
    return (v & 15) <= 9 && (v >> 4) <= 9 ? (v >> 4) * 10 + (v & 15) : -1;
}
uint8_t toBcd(int v) { return uint8_t((v / 10) * 16 + v % 10); }
bool leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
int daysInMonth(int month, int year) {
    static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}
// Bounded 2000-2099 conversion avoids changing process-global TZ while parsing.
int64_t calendarToUtc(int year, int month, int day, int hour, int minute, int second) {
    int64_t days = 10957;  // Days from Unix epoch to 2000-01-01.
    for (int y = 2000; y < year; ++y) days += leap(y) ? 366 : 365;
    for (int m = 1; m < month; ++m) days += daysInMonth(m, year);
    return (days + day - 1) * 86400 + hour * 3600 + minute * 60 + second;
}
}

bool Rx8130::begin(i2c_master_bus_handle_t bus) {
    i2c_device_config_t cfg{};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = 0x32;
    cfg.scl_speed_hz = 100000;
    if (i2c_master_bus_add_device(bus, &cfg, &device_) != ESP_OK) return false;
    uint8_t control = 0;
    return read(Control, &control, 1);
}
bool Rx8130::read(uint8_t reg, uint8_t* bytes, size_t count) {
    return device_ && i2c_master_transmit_receive(device_, &reg, 1, bytes, count, 20) == ESP_OK;
}
bool Rx8130::write(uint8_t reg, const uint8_t* bytes, size_t count) {
    if (!device_ || count > 7) return false;
    uint8_t packet[8]{};
    packet[0] = reg;
    std::memcpy(packet + 1, bytes, count);
    return i2c_master_transmit(device_, packet, count + 1, 20) == ESP_OK;
}
bool Rx8130::readUtc(int64_t& epoch) {
    uint8_t flags = 0, control = 0, date[7]{};
    if (!read(Flags, &flags, 1) || (flags & VoltageLow) ||
        !read(Control, &control, 1) || (control & Stop) || !read(Seconds, date, 7)) return false;
    const int second = fromBcd(date[0] & 0x7f), minute = fromBcd(date[1] & 0x7f);
    const int hour = fromBcd(date[2] & 0x3f), day = fromBcd(date[4] & 0x3f);
    const int month = fromBcd(date[5] & 0x1f), y = fromBcd(date[6]);
    if (y < 0 || second < 0 || second > 59 || minute < 0 || minute > 59 ||
        hour < 0 || hour > 23 || month < 1 || month > 12 || day < 1 ||
        day > daysInMonth(month, 2000 + y)) return false;
    epoch = calendarToUtc(2000 + y, month, day, hour, minute, second);
    return true;
}
bool Rx8130::writeUtc(int64_t epoch) {
    // RTC's two-digit year has an explicit 2000-2099 interpretation.
    if (epoch < 946684800LL || epoch >= 4102444800LL) return false;
    const time_t value = static_cast<time_t>(epoch);
    tm utc{};
    if (!gmtime_r(&value, &utc)) return false;
    uint8_t control = 0;
    if (!read(Control, &control, 1)) return false;
    const uint8_t stopped = control | Stop, running = control & ~Stop;
    if (!write(Control, &stopped, 1)) return false;
    const uint8_t date[] = {toBcd(utc.tm_sec), toBcd(utc.tm_min), toBcd(utc.tm_hour),
        uint8_t(1u << utc.tm_wday), toBcd(utc.tm_mday), toBcd(utc.tm_mon + 1), toBcd(utc.tm_year - 100)};
    const bool written = write(Seconds, date, sizeof(date));
    // Always attempt restart, including if the date write failed.
    const bool restarted = write(Control, &running, 1);
    if (!written || !restarted) return false;
    uint8_t flags = 0;
    if (!read(Flags, &flags, 1)) return false;
    flags &= ~VoltageLow;
    if (!write(Flags, &flags, 1)) return false;
    int64_t readback = 0;
    return readUtc(readback) && readback >= epoch && readback <= epoch + 1;
}
