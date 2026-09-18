#include "board_vibration.h"
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

struct FakeMotor {
    std::vector<uint8_t> duties;
    std::vector<uint16_t> frequencies;
    std::deque<bool> dutyResults;
    bool frequencyResult = true;
    bool setDuty(uint8_t duty) {
        duties.push_back(duty);
        if (dutyResults.empty()) return true;
        const bool result = dutyResults.front();
        dutyResults.pop_front();
        return result;
    }
    bool setFrequency(uint16_t frequency) {
        frequencies.push_back(frequency);
        return frequencyResult;
    }
};

int main() {
    using board::InputVibration;
    {
        FakeMotor motor;
        InputVibration vibration;
        assert(!vibration.available() && !vibration.active());
        vibration.init(motor, 100);
        assert(vibration.available() && !vibration.active());
        assert(motor.duties == std::vector<uint8_t>{0});
        assert(motor.frequencies == std::vector<uint16_t>{5000});
        for (uint32_t now = 101; now < 500; ++now) vibration.poll(false, motor, now);
        vibration.set(false, false, motor, 500);
        assert(motor.duties.size() == 1); // No idle bus writes.

        vibration.set(true, true, motor, 600);
        assert(vibration.active());
        assert(motor.duties.back() == 40);
        vibration.set(true, true, motor, 1000); // A held poll cannot extend deadline.
        vibration.poll(true, motor, 1999);
        assert(vibration.active() && motor.duties.size() == 2);
        vibration.poll(true, motor, 2000);
        assert(!vibration.active() && motor.duties.back() == 0);
        vibration.set(true, true, motor, 2500);
        vibration.poll(true, motor, 2600);
        assert(!vibration.active() && motor.duties.size() == 3);
        vibration.set(false, false, motor, 2700);
        vibration.set(true, true, motor, 2800);
        assert(vibration.active() && motor.duties.back() == 40);
        vibration.set(false, true, motor, 2801); // Navigation/cancellation while held.
        assert(!vibration.active() && motor.duties.back() == 0);
        assert(motor.frequencies.size() == 1);
    }
    {
        FakeMotor motor;
        InputVibration vibration;
        vibration.init(motor, 0);
        vibration.set(true, false, motor, 10);
        assert(!vibration.active() && motor.duties.size() == 1);
        vibration.poll(false, motor, 20);
        vibration.set(true, true, motor, 30);
        assert(vibration.active());
        vibration.poll(false, motor, 31); // Sensor failure or release stops independently.
        assert(!vibration.active() && motor.duties.back() == 0);
        vibration.set(true, true, motor, 40); // New valid physical or injected contact.
        assert(vibration.active());
    }
    {
        FakeMotor motor;
        InputVibration vibration;
        const uint32_t start = UINT32_MAX - 500;
        vibration.init(motor, start - 10);
        vibration.set(true, true, motor, start);
        vibration.poll(true, motor, start + 1399);
        assert(vibration.active());
        vibration.poll(true, motor, start + 1400);
        assert(!vibration.active());
    }
    {
        FakeMotor motor;
        InputVibration vibration;
        vibration.init(motor, 0);
        motor.dutyResults = {false, false, false, true};
        vibration.set(true, true, motor, 50);
        assert(vibration.active()); // ON may have applied before its readback failed.
        assert((motor.duties == std::vector<uint8_t>{0, 40, 0}));
        for (uint32_t now = 51; now < 150; ++now) {
            vibration.poll(true, motor, now);
            vibration.set(false, true, motor, now);
        }
        assert(motor.duties.size() == 3); // Repeated cancellation is rate limited too.
        vibration.poll(true, motor, 150);
        assert(vibration.active() && motor.duties.size() == 4);
        vibration.poll(true, motor, 249);
        assert(motor.duties.size() == 4);
        vibration.poll(true, motor, 250);
        assert(!vibration.active() && motor.duties.size() == 5);
        assert(motor.duties.back() == 0);
    }
    {
        FakeMotor motor;
        InputVibration vibration;
        vibration.init(motor, 0);
        motor.dutyResults = {false, true};
        vibration.set(true, true, motor, 100);
        assert(!vibration.active());
        vibration.set(true, true, motor, 200);
        vibration.poll(true, motor, 300);
        assert((motor.duties == std::vector<uint8_t>{0, 40, 0}));
        vibration.set(false, false, motor, 400);
        vibration.set(true, true, motor, 500);
        assert(vibration.active()); // A new input may try again, never an automatic retry.
    }
    {
        FakeMotor motor;
        InputVibration vibration;
        vibration.init(motor, 0);
        vibration.set(true, true, motor, UINT32_MAX - 100);
        motor.dutyResults = {false, true};
        vibration.set(false, true, motor, UINT32_MAX - 50);
        assert(vibration.active());
        vibration.poll(false, motor, 48);
        assert(vibration.active());
        vibration.poll(false, motor, 49);
        assert(!vibration.active()); // Failed-OFF retry also survives timer wrap.
    }
    {
        FakeMotor motor;
        motor.dutyResults = {false, true};
        InputVibration vibration;
        vibration.init(motor, 500);
        assert(!vibration.available() && vibration.active());
        assert(motor.frequencies.empty());
        vibration.set(true, true, motor, 510);
        assert(motor.duties.size() == 1);
        vibration.poll(false, motor, 600);
        assert(!vibration.active() && !vibration.available());
        assert((motor.duties == std::vector<uint8_t>{0, 0}));
    }
    {
        FakeMotor motor;
        motor.frequencyResult = false;
        InputVibration vibration;
        vibration.init(motor, 0);
        assert(!vibration.available() && !vibration.active());
        vibration.set(true, true, motor, 100);
        vibration.poll(true, motor, 200);
        assert(motor.duties == std::vector<uint8_t>{0});
    }
    std::cout << "Input vibration: startup, bounds, cancellation, wrap and I2C failures passed\n";
}
