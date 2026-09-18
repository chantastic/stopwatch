// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace board {
// Main-task input feedback using the existing expander PWM driver. A software
// deadline limits a held contact; it cannot guarantee shutoff during a stalled
// main task or failed I2C bus. Motor methods return the driver's checked result.
class InputVibration {
public:
    static constexpr uint16_t FrequencyHz = 5000;
    static constexpr uint8_t DutyPercent = 40;
    static constexpr uint32_t MaxHoldMs = 1400;
    static constexpr uint32_t OffRetryMs = 100;

    template<class Motor>
    void init(Motor& motor, uint32_t now) {
        available_ = requested_ = false;
        // PWM can survive an MCU restart. Until a checked zero write succeeds,
        // report possibly active and keep trying to turn it off in poll().
        active_ = stopping_ = true;
        attemptOff(motor, now);
        if (!active_) available_ = motor.setFrequency(FrequencyHz);
    }

    template<class Motor>
    void set(bool pressed, bool contactValid, Motor& motor, uint32_t now) {
        if (!pressed) {
            requested_ = false;
            stop(motor, now);
            return;
        }
        if (requested_) return;
        requested_ = true;
        if (!contactValid || !available_ || active_) return;
        startedAt_ = now;
        active_ = true;
        if (!motor.setDuty(DutyPercent)) {
            // A failed readback may follow a successful ON write. Do not retry
            // ON automatically; immediately issue a checked zero instead.
            stop(motor, now);
        }
    }

    template<class Motor>
    void poll(bool contactValid, Motor& motor, uint32_t now) {
        if (!contactValid) requested_ = false;
        if (!active_) return;
        if (!contactValid || uint32_t(now - startedAt_) >= MaxHoldMs) {
            stop(motor, now);
        }
        if (stopping_ && uint32_t(now - lastOffAttempt_) >= OffRetryMs) {
            attemptOff(motor, now);
        }
    }

    bool available() const { return available_; }
    // True also means a failed write/readback left ON possible, pending a
    // checked OFF. This is conservative driver state, not motor-current sensing.
    bool active() const { return active_; }

private:
    template<class Motor>
    void stop(Motor& motor, uint32_t now) {
        if (!active_ || stopping_) return;
        stopping_ = true;
        attemptOff(motor, now);
    }

    template<class Motor>
    void attemptOff(Motor& motor, uint32_t now) {
        lastOffAttempt_ = now;
        if (motor.setDuty(0)) active_ = stopping_ = false;
    }

    bool available_ = false, requested_ = false;
    bool active_ = false, stopping_ = false;
    uint32_t startedAt_ = 0, lastOffAttempt_ = 0;
};
} // namespace board
