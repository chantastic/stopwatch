#include "widgets.h"
#include "design_assets.h"
#include "../schedule.h"
#include <array>
#include <limits>

namespace badge::ui {
namespace {
constexpr int RowWidth = 272;
constexpr int ContentWidth = RowWidth - 28; // Padding and a reserved two-pixel border.

lv_obj_t* wrapped_label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    auto* text_object = label(parent, text, 0, 0, ContentWidth, font, color);
    lv_label_set_long_mode(text_object, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(text_object, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(text_object, LV_TEXT_ALIGN_LEFT, 0);
    return text_object;
}

struct ScheduleRow {
    lv_obj_t* box = nullptr;
    lv_obj_t* now = nullptr;
    lv_obj_t* bookmark_icon = nullptr;
    badge_schedule::State state = badge_schedule::State::Unknown;
    bool styled = false;
};

// LVGL still owns the card, clipping, border, opacity and invalidation. These
// tiny native draw primitives cut the design's square eight-pixel corner steps.
void draw_notches(lv_event_t* event) {
    auto& row = *static_cast<ScheduleRow*>(lv_event_get_user_data(event));
    lv_area_t bounds;
    lv_obj_get_coords(row.box, &bounds);
    auto* layer = lv_event_get_layer(event);
    lv_draw_rect_dsc_t shape;
    lv_draw_rect_dsc_init(&shape);
    shape.bg_color = lv_color_black();
    shape.bg_opa = LV_OPA_COVER;
    auto rect = [&](int x, int y, int width, int height) {
        lv_area_t area = {x, y, x + width - 1, y + height - 1};
        lv_draw_rect(layer, &shape, &area);
    };
    const int xs[] = {bounds.x1, bounds.x2 - 7};
    const int ys[] = {bounds.y1, bounds.y2 - 7};
    for (int x : xs) for (int y : ys) rect(x, y, 8, 8);
    if (row.state != badge_schedule::State::OnNow) return;
    shape.bg_color = white();
    for (int side = 0; side < 2; ++side) {
        const int x = side ? bounds.x2 - 7 : bounds.x1;
        const int vertical_x = side ? x : x + 6;
        rect(vertical_x, bounds.y1, 2, 8);
        rect(x, bounds.y1 + 6, 8, 2);
        rect(vertical_x, bounds.y2 - 7, 2, 8);
        rect(x, bounds.y2 - 7, 8, 2);
    }
}

lv_obj_t* bookmark(lv_obj_t* parent, std::function<void()> action) {
    auto* target = container(parent, ContentWidth - 36, -8, 40, 40);
    lv_obj_set_ext_click_area(target, 0);
    on_tap(target, std::move(action));
    auto* icon = lv_image_create(target);
    lv_image_set_src(icon, &supplied_bookmark_outline);
    lv_obj_center(icon);
    lv_obj_set_style_image_recolor(icon, white(), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    return icon;
}
}

class SchedulePage final : public PageView {
public:
    SchedulePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Schedule", 84, 52, 300, &font_sans_24);
        subtitle_ = label(root_, "", 64, 87, 340, &font_mono_12, muted());
        list_ = container(root_, 98, 126, RowWidth, 254);
        lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_bottom(list_, 0, 0);
        lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_, 12, 0);

        for (size_t i = 0; i < badge_schedule::Items.size(); ++i) {
            const auto& item = badge_schedule::Items[i];
            auto& row = rows_[i];
            row.box = container(list_, 0, 0, RowWidth, 1);
            lv_obj_set_height(row.box, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(row.box, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_all(row.box, 12, 0);
            lv_obj_set_style_pad_row(row.box, 8, 0);
            lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row.box, 0, 0);
            // Time and bookmark changes never change row geometry or discard
            // the attendee's current native scrolling position.
            lv_obj_set_style_border_width(row.box, 2, 0);
            lv_obj_set_style_border_color(row.box, white(), 0);
            lv_obj_add_event_cb(row.box, draw_notches, LV_EVENT_DRAW_MAIN_END, &row);
            auto* header = container(row.box, 0, 0, ContentWidth, 18);
            lv_obj_add_flag(header, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
            auto* time = label(header, item.time, 0, 0, 110, &font_mono_12, white());
            lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_LEFT, 0);
            row.now = label(header, "", 112, 0, 82, &font_mono_12, white());
            lv_obj_set_style_text_align(row.now, LV_TEXT_ALIGN_RIGHT, 0);
            row.bookmark_icon = bookmark(header, [this, i] {
                context_.model.schedule_bookmarks ^= uint16_t(1) << i;
                if (context_.callbacks.bookmark) context_.callbacks.bookmark(int(i));
                update_bookmarks();
            });
            wrapped_label(row.box, item.title, &font_sans_20, white());
            if (item.detail && item.detail[0]) wrapped_label(row.box, item.detail, &font_mono_12, muted());
        }
        label(root_, "Swipe up or down", 84, 397, 300, &font_mono_12, muted());
        update();
        lv_obj_update_layout(list_);
        const int current = context_.model.schedule_current;
        if (current >= 0 && current < int(rows_.size()) && rows_[current].state == badge_schedule::State::OnNow) {
            lv_obj_scroll_to_view(rows_[current].box, LV_ANIM_OFF);
        }
    }

    void update() override {
        update_bookmarks();
        const int minute = context_.model.clock_valid ? context_.model.schedule_minute : -1;
        if (minute == minute_) return;
        minute_ = minute;
        set_text(subtitle_, minute < 0 ? "Set time in Settings" : "All times local");
        for (size_t i = 0; i < rows_.size(); ++i) {
            auto& row = rows_[i];
            const auto state = badge_schedule::state(i, minute);
            if (row.styled && row.state == state) continue;
            row.state = state;
            row.styled = true;
            const bool current = state == badge_schedule::State::OnNow;
            set_text(row.now, current ? "On now" : "");
            lv_obj_set_style_border_opa(row.box, current ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(row.box, lv_color_hex(0x151515), 0);
            lv_obj_set_style_opa(row.box, state == badge_schedule::State::Passed ? LV_OPA_50 : LV_OPA_COVER, 0);
        }
    }
private:
    void update_bookmarks() {
        const uint16_t mask = context_.model.schedule_bookmarks;
        if (bookmark_mask_ == mask) return;
        bookmark_mask_ = mask;
        for (size_t i = 0; i < rows_.size(); ++i)
            lv_image_set_src(rows_[i].bookmark_icon, mask & (uint16_t(1) << i) ?
                &supplied_bookmark : &supplied_bookmark_outline);
    }
    int minute_ = -2;
    uint16_t bookmark_mask_ = std::numeric_limits<uint16_t>::max();
    lv_obj_t* list_ = nullptr;
    lv_obj_t* subtitle_ = nullptr;
    std::array<ScheduleRow, badge_schedule::Items.size()> rows_{};
};
std::unique_ptr<PageView> make_schedule(Context& c, lv_obj_t* p) { return std::make_unique<SchedulePage>(c, p); }
} // namespace badge::ui
