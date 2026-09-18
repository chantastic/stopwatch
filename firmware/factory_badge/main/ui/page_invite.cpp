#include "widgets.h"
#include "intro_loop.h"
#include "../morse_unlock.h"
#include <cstdlib>

namespace badge::ui {
class InvitePage final : public PageView {
public:
    InvitePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        title_ = label(root_, "Developers", 54, 78, 360, &font_sans_32, cream());
        subtitle_ = label(root_, "After Dark", 54, 120, 360, &font_sans_32, cream());
        lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(root_, panel(), LV_STATE_PRESSED);
        prompt_ = label(root_, "", 80, 216, 308, &font_sans_24, white());
        lv_label_set_long_mode(prompt_, LV_LABEL_LONG_WRAP);
        footer_ = label(root_, "", 64, 367, 340, &font_mono_semibold_12, muted());
        // Ordinary on_tap rejects long presses. This surface instead feeds
        // native press durations to the existing bounded Morse recognizer.
        // Non-clickable labels pass through to the whole page; the separate
        // chrome layer retains its native previous/next controls.
        lv_obj_add_event_cb(root_, [](lv_event_t* event) {
            const auto code = lv_event_get_code(event);
            if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
                code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST &&
                code != LV_EVENT_GESTURE && code != LV_EVENT_SCROLL_BEGIN) return;
            // In particular, deletion occurs from PageView's destructor after
            // InvitePage members are gone; never dereference them for DELETE.
            auto& self = *static_cast<InvitePage*>(lv_event_get_user_data(event));
            if (lv_event_get_target(event) != self.root_) return;
            self.contact(event);
        }, LV_EVENT_ALL, this);
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
    ~InvitePage() override { lv_anim_delete(this, reveal_word); }

    void cancel_input() override { cancel(); }
    void update() override {
        const bool locked = !context_.model.after_dark_unlocked;
        if (!locked || context_.setup || context_.touch_test || context_.reset) cancel();
        else sample(); // Also observes the quiet pause after the final release.
        const bool revealed = context_.model.after_dark_unlocked;
        if (!revealing())
            set_text(prompt_, revealed ? "You're invited" : "tap the code to reveal a secret invitation");
        set_text(footer_, revealed ? "Invite details coming soon" : "");
        // No sample QR: the real invitation URL is still pending.
        if (displayed_state_ != int(revealed)) {
            const bool code_accepted = revealed && submitted_ && displayed_state_ == 0;
            displayed_state_ = int(revealed);
            set_hidden(title_, !revealed);
            set_hidden(subtitle_, !revealed);
            lv_obj_set_y(prompt_, revealed ? 249 : 216);
            if (revealed) lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
            else lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
            if (code_accepted) celebrate();
        }
        if (celebration_done_) {
            // READY runs inside LVGL's decoder; release its object and timer
            // here, after that callback has returned to the ordinary UI tick.
            lv_obj_delete(celebration_);
            celebration_ = nullptr;
            celebration_done_ = false;
        }
    }
private:
    void cancel() {
        held_ = false;
        armed_ = false;
        morse_.reset();
        lv_obj_remove_state(root_, LV_STATE_PRESSED);
    }
    void celebrate() {
        celebration_ = lv_gif_create(root_);
        lv_gif_set_color_format(celebration_, LV_COLOR_FORMAT_RGB565);
        lv_gif_set_src(celebration_, &intro_loop);
        lv_gif_set_loop_count(celebration_, 1);
        lv_obj_center(celebration_);
        lv_obj_move_to_index(celebration_, 0);
        lv_obj_remove_flag(celebration_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(celebration_, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(celebration_, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(celebration_, [](lv_event_t* event) {
            static_cast<InvitePage*>(lv_event_get_user_data(event))->celebration_done_ = true;
        }, LV_EVENT_READY, this);
        // LVGL advances the words; the page destructor cancels this animation
        // before its callback can outlive the view. The GIF keeps its own timing.
        reveal_stage_ = -1;
        lv_anim_t words;
        lv_anim_init(&words);
        lv_anim_set_var(&words, this);
        lv_anim_set_exec_cb(&words, reveal_word);
        lv_anim_set_values(&words, 0, 3);
        lv_anim_set_duration(&words, 2100);
        lv_anim_start(&words);
    }
    bool revealing() const { return reveal_stage_ >= 0 && reveal_stage_ < 3; }
    static void reveal_word(void* view, int32_t stage) {
        auto& self = *static_cast<InvitePage*>(view);
        if (self.reveal_stage_ == stage) return;
        self.reveal_stage_ = stage;
        const bool active = self.revealing();
        set_hidden(self.title_, active);
        set_hidden(self.subtitle_, active);
        set_hidden(self.footer_, active);
        set_font(self.prompt_, active ? &font_sans_32 : &font_sans_24);
        lv_obj_set_y(self.prompt_, active ? 212 : 249);
        constexpr const char* words[] = {"You're", "Invited", "To", "You're invited"};
        set_text(self.prompt_, words[stage]);
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
    lv_obj_t *title_ = nullptr, *subtitle_ = nullptr, *prompt_ = nullptr,
             *footer_ = nullptr, *celebration_ = nullptr;
    MorseUnlock morse_;
    lv_point_t start_{};
    int displayed_state_ = -1, reveal_stage_ = 3;
    bool held_ = false, armed_ = false, submitted_ = false, celebration_done_ = false;
};
std::unique_ptr<PageView> make_after_dark(Context& c, lv_obj_t* p) { return std::make_unique<InvitePage>(c, p); }
} // namespace badge::ui
