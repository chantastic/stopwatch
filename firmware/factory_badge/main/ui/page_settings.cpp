#include "widgets.h"
#include <algorithm>
#include <array>

namespace badge::ui {
class SettingsPage final : public PageView {
public:
    SettingsPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Settings", 84, 66, 300, &lv_font_montserrat_24);
        battery_ = label(root_, "", 64, 103, 340, &lv_font_montserrat_18, muted());
        label(root_, "Brightness", 134, 134, 200, &lv_font_montserrat_14, muted());
        button(root_, "-", 94, 155, 64, 44, [this] { brightness(-10); });
        button(root_, "+", 310, 155, 64, 44, [this] { brightness(10); });
        brightness_ = label(root_, "", 162, 164, 144, &lv_font_montserrat_24);
        date_ = label(root_, "", 68, 212, 332, &lv_font_montserrat_18);
        label(root_, "Sync time in phone setup", 84, 236, 300, &lv_font_montserrat_14, muted());
        button(root_, "Connect phone", 78, 260, 152, 46, [this] { request_setup(context_); });
        button(root_, "Touch test", 238, 260, 152, 46, [] { ui_show_touch_test(); });
        label(root_, "Orientation", 104, 324, 260, &lv_font_montserrat_14, muted());
        constexpr const char* names[] = {"Free", "Default", "180°"};
        for (int i = 0; i < 3; ++i) {
            orientation_[i] = button(root_, names[i], 91 + i * 98, 347, 90, 44, [this, i] {
                context_.model.orientation = static_cast<Orientation>(i);
                if (context_.callbacks.orientation) context_.callbacks.orientation(context_.model.orientation);
                update();
            });
        }
        update();
    }
    void update() override {
        set_text(battery_, battery_text(context_.model.battery_percent));
        set_text(brightness_, std::to_string(context_.model.brightness_percent) + "%");
        set_text(date_, context_.model.clock_valid ? context_.model.date_text : "Date / time not set");
        if (orientation_value_ != int(context_.model.orientation)) {
            orientation_value_ = int(context_.model.orientation);
            for (int i = 0; i < 3; ++i) {
                const bool selected = i == orientation_value_;
                lv_obj_set_style_border_color(orientation_[i], accent(), 0);
                lv_obj_set_style_border_width(orientation_[i], selected ? 2 : 0, 0);
                lv_obj_set_style_bg_color(orientation_[i], selected ? lv_color_hex(0x363044) : panel(), 0);
            }
        }
    }
private:
    void brightness(int delta) {
        context_.model.brightness_percent = std::clamp(context_.model.brightness_percent + delta, 10, 100);
        if (context_.callbacks.brightness) context_.callbacks.brightness(context_.model.brightness_percent);
        update();
    }
    lv_obj_t* battery_ = nullptr;
    lv_obj_t* brightness_ = nullptr;
    lv_obj_t* date_ = nullptr;
    int orientation_value_ = -1;
    std::array<lv_obj_t*, 3> orientation_{};
};
std::unique_ptr<PageView> make_settings(Context& c, lv_obj_t* p) { return std::make_unique<SettingsPage>(c, p); }
} // namespace badge::ui
