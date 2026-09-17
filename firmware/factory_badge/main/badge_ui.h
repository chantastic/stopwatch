#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <lvgl.h>

namespace badge {

enum class Orientation : uint8_t { Free, Default, UpsideDown };

// The main task owns this model and all UI calls. Avatar memory must remain valid
// until the next ui_update(), including while LVGL renders the current frame.
struct UiModel {
    std::string name;
    std::array<std::string, 3> socials;
    std::string clock_text = "--:--";
    std::string date_text = "Date / time not set";
    const uint16_t* avatar = nullptr;
    uint16_t avatar_width = 0;
    uint16_t avatar_height = 0;
    uint32_t profile_revision = 0;
    int battery_percent = -1;
    int brightness_percent = 50;
    Orientation orientation = Orientation::Free;
    int selected_network = 0;
    bool settings_pending = false;
    bool clock_valid = false;
    bool after_dark_unlocked = false;
    int schedule_minute = -1; // Local minute of day; -1 when the clock is unset.
    int schedule_current = -1;
};

struct UiCallbacks {
    std::function<void()> request_setup;
    std::function<void()> close_setup;
    std::function<void(int)> brightness; // Absolute percentage, 10 through 100.
    std::function<void(Orientation)> orientation;
    std::function<void(int)> network;
};

struct UiTouchSample {
    bool sample = false;
    bool pressed = false;
    bool sensor = true;
    int raw_x = 0;
    int raw_y = 0;
    int x = 0;
    int y = 0;
    uint8_t rotation = 0;
};

void ui_init(lv_display_t* display, UiCallbacks callbacks);
void ui_update(const UiModel& model);
void ui_tick(uint32_t now_ms);
void ui_rotation_changed();
void ui_page(int delta);
void ui_button(bool both, int delta);
void ui_open_after_dark(); // Successful code entry; never interrupts a modal.
void ui_show_setup(const std::string& ssid, const std::string& password,
                   const std::string& ip = "192.168.4.1", const std::string& status = "");
void ui_close_setup(bool saved);
void ui_show_touch_test();
void ui_close_touch_test();
void ui_touch_sample(int raw_x, int raw_y, int x, int y, bool pressed,
                     uint8_t rotation, bool sensor = true);
UiTouchSample ui_touch_state();
bool ui_touch_test_active();
bool ui_setup_active();
int ui_page_index();
int ui_page_count(); // Visible pages; stable page IDs do not change on unlock.

} // namespace badge
