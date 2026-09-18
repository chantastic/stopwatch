#include "widgets.h"
#include <algorithm>
#include <array>

namespace badge::ui {
class SettingsPage final : public PageView {
public:
    SettingsPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Settings", 84, 64, 300, &font_sans_24, cream());
        battery_ = label(root_, "", 64, 98, 340, &font_mono_semibold_12, muted());
        auto* brightness_title = label(root_, "Brightness", 87, 157, 98, &font_sans_14, cream());
        lv_obj_set_style_text_align(brightness_title, LV_TEXT_ALIGN_LEFT, 0);
        auto* minus = button(root_, "", 186, 144, 62, 44, [this] { brightness(-10); });
        auto* plus = button(root_, "", 318, 144, 62, 44, [this] { brightness(10); });
        // Figma uses geometric marks, independent of the text face.
        auto bar = [](lv_obj_t* parent, int width, int height) {
            auto* mark = container(parent, 0, 0, width, height);
            lv_obj_set_style_bg_color(mark, white(), 0);
            lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
            lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_center(mark);
        };
        bar(minus, 13, 2);
        bar(plus, 11, 2);
        bar(plus, 2, 11);
        brightness_ = label(root_, "", 249, 156, 68, &font_sans_16, cream());
        auto* orientation_title = label(root_, "Orientation", 87, 229, 98, &font_sans_14, cream());
        lv_obj_set_style_text_align(orientation_title, LV_TEXT_ALIGN_LEFT, 0);
        constexpr const char* names[] = {"Free", "Default", "180°"};
        constexpr int positions[] = {186, 256, 324};
        constexpr int widths[] = {58, 56, 56};
        for (int i = 0; i < 3; ++i) {
            orientation_[i] = button(root_, names[i], positions[i], 216, widths[i], 44, [this, i] {
                context_.model.orientation = static_cast<Orientation>(i);
                if (context_.callbacks.orientation) context_.callbacks.orientation(context_.model.orientation);
                update();
            });
            set_font(lv_obj_get_child(orientation_[i], 0), &font_sans_12);
            lv_obj_align(lv_obj_get_child(orientation_[i], 0), LV_ALIGN_CENTER, 0, -1);
            lv_obj_set_style_text_color(lv_obj_get_child(orientation_[i], 0), white(), 0);
        }
        for (int y : {201, 273}) {
            for (int x = 86; x < 380; x += 6) {
                auto* dash = container(root_, std::max(87, x), y, x == 86 ? 2 : 3, 1);
                lv_obj_remove_flag(dash, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_bg_color(dash, lv_color_hex(0x171717), 0);
                lv_obj_set_style_bg_opa(dash, LV_OPA_COVER, 0);
            }
        }
        date_ = label(root_, "", 87, 291, 293, &font_sans_14, cream());
        lv_obj_set_style_text_align(date_, LV_TEXT_ALIGN_LEFT, 0);
        auto* hint = label(root_, "Edit badge or sync time", 87, 308, 293, &font_mono_semibold_12, muted());
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_LEFT, 0);
        auto* connect = button(root_, "Connect Phone", 87, 339, 293, 44, [this] { request_setup(context_); });
        set_font(lv_obj_get_child(connect, 0), &font_sans_14);
        lv_obj_set_style_text_color(lv_obj_get_child(connect, 0), white(), 0);
        auto* test = button(root_, "Touch test", 110, 388, 116, 32, [] { ui_show_touch_test(); });
        auto* reset = button(root_, "Reset badge", 242, 388, 116, 32, [] { ui_show_reset(); });
        for (auto* control : {test, reset}) {
            lv_obj_set_style_bg_opa(control, LV_OPA_TRANSP, 0);
            auto* title = lv_obj_get_child(control, 0);
            set_font(title, &font_mono_semibold_12);
            lv_obj_set_style_text_color(title, muted(), 0);
        }
        update();
    }
    void update() override {
        set_text(battery_, battery_text(context_.model.battery_percent));
        set_text(brightness_, std::to_string(context_.model.brightness_percent) + "%");
        auto date = context_.model.date_text;
        if (date.size() > 10 && date[10] == ' ') date.replace(10, 1, " | ");
        set_text(date_, context_.model.clock_valid ? date : "Date / time not set");
        if (orientation_value_ != int(context_.model.orientation)) {
            orientation_value_ = int(context_.model.orientation);
            for (int i = 0; i < 3; ++i) {
                lv_obj_set_style_border_color(orientation_[i], white(), 0);
                lv_obj_set_style_border_width(orientation_[i], i == orientation_value_ ? 1 : 0, 0);
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
