#include "widgets.h"
#include <cmath>

namespace badge::ui {
class InitPage final : public PageView {
public:
    InitPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        brand(root_, 188, true);
        label(root_, "CONFERENCE BADGE", 84, 258, 300, &lv_font_montserrat_14, muted());
        label(root_, "Animation preview", 84, 366, 300, &lv_font_montserrat_14, muted());
        battery_ = label(root_, "", 114, 58, 240, &lv_font_montserrat_14, muted());
        for (int i = 0; i < 24; ++i) {
            const float angle = i * 6.2831853f / 24;
            auto* dot = container(root_, 231 + int(128 * std::cos(angle)), 214 + int(128 * std::sin(angle)), 6, 6);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, white(), 0);
            lv_anim_t animation;
            lv_anim_init(&animation);
            lv_anim_set_var(&animation, dot);
            lv_anim_set_values(&animation, 50, 255);
            lv_anim_set_duration(&animation, 2400);
            lv_anim_set_playback_duration(&animation, 2400);
            lv_anim_set_delay(&animation, i * 100);
            lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
            lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
                lv_obj_set_style_bg_opa(static_cast<lv_obj_t*>(object), value, 0);
            });
            lv_anim_start(&animation);
        }
        update();
    }
    void update() override { set_text(battery_, battery_text(context_.model.battery_percent)); }
private:
    lv_obj_t* battery_ = nullptr;
};
std::unique_ptr<PageView> make_init(Context& c, lv_obj_t* p) { return std::make_unique<InitPage>(c, p); }
} // namespace badge::ui
