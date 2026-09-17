#include "vendor/rx8130/rx8130.h"
#include <cassert>
#include <cstring>
#include <iostream>
uint8_t registers_[256]{};
bool failRead = false, failDateWrite = false;
int writes = 0;
int i2c_master_bus_add_device(void*, const i2c_device_config_t*, void** ptr) { *ptr = registers_; return 0; }
int i2c_master_transmit_receive(void*, const void* tx, size_t, void* rx, size_t n, int) {
 if (failRead) return -1;
 std::memcpy(rx, registers_+*static_cast<const uint8_t*>(tx), n); return 0;
}
int i2c_master_transmit(void*, const void* tx, size_t n, int) {
 const auto p = static_cast<const uint8_t*>(tx); ++writes;
 if (p[0] == 0x10 && failDateWrite) return -1;
 std::memcpy(registers_+p[0], p+1, n-1); return 0;
}
int main() {
 Rx8130 rtc; assert(rtc.begin(nullptr));
 for (int64_t epoch: {946684800LL,951782400LL,1709164800LL,1789657200LL,4102444799LL}) {
  assert(rtc.writeUtc(epoch)); int64_t observed = -1; const int before = writes;
  assert(rtc.readUtc(observed)); assert(observed == epoch); assert(writes == before);
  assert(registers_[0x13] != 0 && (registers_[0x13] & (registers_[0x13]-1)) == 0);
 }
 assert(!rtc.writeUtc(946684799LL)); assert(!rtc.writeUtc(4102444800LL));
 int64_t epoch = -1; registers_[0x15] = 0; assert(!rtc.readUtc(epoch));
 assert(rtc.writeUtc(1709164800LL)); registers_[0x16] = 0x23; assert(!rtc.readUtc(epoch));
 assert(rtc.writeUtc(1709164800LL)); registers_[0x10] = 0x6a; assert(!rtc.readUtc(epoch));
 assert(rtc.writeUtc(1709164800LL)); registers_[0x1d] = 2; assert(!rtc.readUtc(epoch));
 assert(rtc.writeUtc(1709164800LL)); registers_[0x1e] |= 0x40; assert(!rtc.readUtc(epoch));
 failDateWrite = true; assert(!rtc.writeUtc(1709164800LL)); assert(!(registers_[0x1e] & 0x40));
 failDateWrite = false; failRead = true; assert(!rtc.readUtc(epoch)); assert(!rtc.writeUtc(1709164800LL));
 std::cout << "RTC epoch/calendar validation, read-only reads, write failures and restart checks passed\n";
}
