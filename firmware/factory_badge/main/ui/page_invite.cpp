#include "widgets.h"
#include "../morse_unlock.h"
#include <cstdlib>

namespace badge::ui {
class InvitePage final : public PageView {
public:
    InvitePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Developers", 54, 78, 360, &font_sans_32, cream());
        label(root_, "After Dark", 54, 120, 360, &font_sans_32, cream());
        pad_ = container(root_, 156, 195, 156, 156);
        lv_obj_set_style_bg_color(pad_, panel(), 0);
        lv_obj_set_style_bg_opa(pad_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(pad_, muted(), 0);
        lv_obj_set_style_border_width(pad_, 1, 0);
        lv_obj_set_style_bg_color(pad_, lv_color_hex(0x333333), LV_STATE_PRESSED);
        prompt_ = label(pad_, "", 8, 69, 140, &font_mono_12, white());
        footer_ = label(root_, "", 64, 367, 340, &font_mono_semibold_12, muted());
        // Ordinary on_tap rejects long presses. This surface instead feeds
        // native press durations to the existing bounded Morse recognizer.
        lv_obj_add_event_cb(pad_, [](lv_event_t* event) {
            const auto code = lv_event_get_code(event);
            if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
                code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST &&
                code != LV_EVENT_GESTURE && code != LV_EVENT_SCROLL_BEGIN) return;
            // In particular, deletion occurs from PageView's destructor after
            // InvitePage members are gone; never dereference them for DELETE.
            auto& self = *static_cast<InvitePage*>(lv_event_get_user_data(event));
            if (lv_event_get_target(event) != self.pad_) return;
            self.contact(event);
        }, LV_EVENT_ALL, this);
        lv_obj_add_event_cb(root_, [](lv_event_t* event) {
            auto& self = *static_cast<InvitePage*>(lv_event_get_user_data(event));
            if (lv_event_get_target(event) != self.pad_) self.cancel();
        }, LV_EVENT_PRESSED, this);
        // A pusher can enter this page while a finger is already held. That
        // old contact must lift before it can become the first code symbol.
        for (auto* input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input)) {
            if (lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER &&
                lv_indev_get_display(input) == lv_obj_get_display(root_) &&
                lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED)
                lv_indev_wait_release(input);
        }
        update();
    }

    void cancel_input() override { cancel(); }
    void update() override {
        const bool locked = !context_.model.after_dark_unlocked;
        if (!locked || context_.setup || context_.touch_test || context_.reset) cancel();
        else sample(); // Also observes the quiet pause after the final release.
        const bool revealed = context_.model.after_dark_unlocked;
        set_text(prompt_, revealed ? "Coming soon" : "tap code to reveal");
        set_text(footer_, revealed ? "Invite details coming soon" : "");
        // No sample QR: the real invitation URL is still pending.
        if (displayed_state_ != int(revealed)) {
            displayed_state_ = int(revealed);
            lv_obj_set_style_text_color(prompt_, revealed ? muted() : white(), 0);
            if (revealed) lv_obj_remove_flag(pad_, LV_OBJ_FLAG_CLICKABLE);
            else lv_obj_add_flag(pad_, LV_OBJ_FLAG_CLICKABLE);
        }
    }
private:
    void cancel() {
        held_ = false;
        armed_ = false;
        morse_.reset();
        lv_obj_remove_state(pad_, LV_STATE_PRESSED);
    }
    void sample() {
        if (submitted_ || context_.model.after_dark_unlocked) return;
        if (!morse_.update(held_, false, lv_tick_get())) return;
        submitted_ = true;
        auto action = context_.callbacks.unlock_after_dark;
        if (action) action();
    }
    void contact(lv_event_t* event) {
        if (context_.model.after_dark_unlocked || submitted_) return;
        const auto code = lv_event_get_code(event);
        auto* input = lv_indev_active();
        if (code == LV_EVENT_PRESSED && input) {
            lv_indev_get_point(input, &start_);
            held_ = armed_ = true;
            sample();
        } else if (code == LV_EVENT_PRESSING && input && armed_) {
            lv_point_t point;
            lv_indev_get_point(input, &point);
            if (std::abs(point.x - start_.x) > 10 || std::abs(point.y - start_.y) > 10) cancel();
        } else if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_GESTURE || code == LV_EVENT_SCROLL_BEGIN) {
            cancel();
        } else if (code == LV_EVENT_RELEASED && armed_) {
            held_ = armed_ = false;
            sample();
        }
        // LONG_PRESSED is intentionally accepted: holds are Morse dashes.
    }
    lv_obj_t *pad_ = nullptr, *prompt_ = nullptr, *footer_ = nullptr;
    MorseUnlock morse_;
    lv_point_t start_{};
    int displayed_state_ = -1;
    bool held_ = false, armed_ = false, submitted_ = false;
};
std::unique_ptr<PageView> make_after_dark(Context& c, lv_obj_t* p) { return std::make_unique<InvitePage>(c, p); }
} // namespace badge::ui
