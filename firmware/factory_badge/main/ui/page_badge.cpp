#include "widgets.h"
#include "brand_assets.h"
#include <algorithm>
#include <array>

namespace badge::ui {
class BadgePage final : public PageView {
public:
    BadgePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        list_ = container(root_, 72, 61, 324, CardHeight);
        lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_snap_y(list_, LV_SCROLL_SNAP_CENTER);
        lv_obj_remove_flag(list_, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_, 0, 0);
        for (int i = 0; i < 3; ++i) {
            cards_[i] = container(list_, 0, 0, 324, CardHeight);
            lv_obj_add_flag(cards_[i], LV_OBJ_FLAG_SNAPPABLE);
            dots_[i] = container(root_, 399, 275 + i * 16, 5, 5);
            lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(dots_[i], LV_OPA_COVER, 0);
        }
        lv_obj_add_event_cb(list_, [](lv_event_t* event) {
            auto& self = *static_cast<BadgePage*>(lv_event_get_user_data(event));
            const int selected = std::clamp((int(lv_obj_get_scroll_y(self.list_)) + CardHeight / 2) / CardHeight, 0, 2);
            if (selected != self.context_.model.selected_network) {
                self.context_.model.selected_network = selected;
                if (self.context_.callbacks.network) self.context_.callbacks.network(selected);
            }
            self.update_dots();
        }, LV_EVENT_SCROLL_END, this);
        rebuild_cards();
        lv_obj_update_layout(list_);
        scroll_to_model();
    }
    void update() override {
        const auto& model = context_.model;
        if (revision_ != model.profile_revision || name_ != model.name || urls_ != model.socials || avatar_ != model.avatar) rebuild_cards();
        // A user scroll owns its position until it settles. Only a changed model
        // selection (USB/preferences) moves the list programmatically.
        if (selected_ != model.selected_network) scroll_to_model();
        update_dots();
    }
private:
    static constexpr int CardHeight = 338;
    void update_dots() {
        selected_ = std::clamp(context_.model.selected_network, 0, 2);
        if (dot_selection_ == selected_) return;
        dot_selection_ = selected_;
        for (int i = 0; i < 3; ++i) lv_obj_set_style_bg_color(dots_[i], i == selected_ ? white() : muted(), 0);
    }
    void scroll_to_model() {
        selected_ = std::clamp(context_.model.selected_network, 0, 2);
        lv_obj_scroll_to_y(list_, selected_ * CardHeight, LV_ANIM_OFF);
        update_dots();
    }
    void avatar(lv_obj_t* card) {
        auto* frame = container(card, 122, 31, 80, 80);
        lv_obj_set_style_radius(frame, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_clip_corner(frame, true, 0);
        lv_obj_set_style_bg_color(frame, panel(), 0);
        lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
        if (avatar_ && image_.header.w && image_.header.h) {
            auto* photo = lv_image_create(frame);
            lv_image_set_src(photo, &image_);
            lv_image_set_scale(photo, 256 * 80 / image_.header.w);
            lv_obj_center(photo);
        } else {
            auto* head = container(frame, 29, 14, 22, 22);
            lv_obj_set_style_bg_color(head, muted(), 0);
            lv_obj_set_style_bg_opa(head, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(head, LV_RADIUS_CIRCLE, 0);
            auto* body = container(frame, 18, 43, 44, 25);
            lv_obj_set_style_bg_color(body, muted(), 0);
            lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(body, 16, 0);
        }
    }
    void rebuild_cards() {
        const auto& model = context_.model;
        revision_ = model.profile_revision;
        name_ = model.name;
        urls_ = model.socials;
        avatar_ = model.avatar;
#if LV_CACHE_DEF_SIZE > 0
        lv_image_cache_drop(&image_);
#endif
        image_ = {};
        image_.header.magic = LV_IMAGE_HEADER_MAGIC;
        image_.header.cf = LV_COLOR_FORMAT_RGB565;
        image_.header.w = model.avatar_width;
        image_.header.h = model.avatar_height;
        image_.header.stride = model.avatar_width * 2;
        image_.data_size = model.avatar_width * model.avatar_height * 2;
        image_.data = reinterpret_cast<const uint8_t*>(avatar_);
        for (int i = 0; i < 3; ++i) {
            auto* card = cards_[i];
            lv_obj_clean(card);
            brand(card, 3);
            avatar(card);
            label(card, name_.empty() ? "Your name" : name_.c_str(), 6, 117, 312, &lv_font_montserrat_24);
            if (i == 0) {
                auto* mark = lv_image_create(card);
                lv_image_set_src(mark, &github_icon);
                lv_obj_set_pos(mark, 73, 150);
                lv_obj_set_style_image_recolor(mark, white(), 0);
                lv_obj_set_style_image_recolor_opa(mark, LV_OPA_COVER, 0);
            } else {
                auto* mark = container(card, 73, 151, 28, 25);
                lv_obj_set_style_bg_color(mark, white(), 0);
                lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
                lv_obj_set_style_radius(mark, i == 2 ? 3 : LV_RADIUS_CIRCLE, 0);
                label(mark, i == 1 ? "X" : "in", 0, 4, 28, &lv_font_montserrat_14, lv_color_black());
            }
            label(card, NetworkNames[i], 104, 153, 162, &lv_font_montserrat_18);
            if (urls_[i].empty()) {
                label(card, "No account yet", 12, 212, 300, &lv_font_montserrat_18, muted());
                button(card, "Tap to configure", 54, 253, 216, 48, [this] { request_setup(context_); });
                label(card, "Swipe for other networks", 10, 315, 304, &lv_font_montserrat_14, muted());
            } else {
                auto* code = qr(card, urls_[i], 89, 184, 146);
                on_tap(code, [this, i] { expand(i); });
            }
        }
    }
    void expand(int network) {
        if (expanded_) return;
        expanded_ = container(root_, 72, 61, 324, 342);
        lv_obj_set_style_bg_color(expanded_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(expanded_, LV_OPA_COVER, 0);
        label(expanded_, NetworkNames[network], 22, 6, 280, &lv_font_montserrat_18);
        qr(expanded_, urls_[network], 29, 43, 266);
        label(expanded_, "Tap to close", 22, 319, 280, &lv_font_montserrat_14, muted());
        on_tap(expanded_, [this] {
            lv_obj_delete(expanded_);
            expanded_ = nullptr;
        });
    }
    lv_obj_t* list_ = nullptr;
    lv_obj_t* expanded_ = nullptr;
    std::array<lv_obj_t*, 3> cards_{};
    std::array<lv_obj_t*, 3> dots_{};
    uint32_t revision_ = 0;
    int selected_ = -1;
    int dot_selection_ = -1;
    const uint16_t* avatar_ = nullptr;
    lv_image_dsc_t image_{};
    std::string name_;
    std::array<std::string, 3> urls_;
};
std::unique_ptr<PageView> make_badge(Context& c, lv_obj_t* p) { return std::make_unique<BadgePage>(c, p); }
} // namespace badge::ui
