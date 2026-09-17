#include "widgets.h"
#include "../schedule.h"
#include <array>

namespace badge::ui {
namespace {
constexpr int RowWidth = 292;
constexpr int ContentWidth = RowWidth - 28; // Padding plus a fixed two-pixel border.

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
    badge_schedule::State state = badge_schedule::State::Unknown;
    bool styled = false;
};
}

class SchedulePage final : public PageView {
public:
    SchedulePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Schedule", 84, 68, 300, &lv_font_montserrat_24);
        subtitle_ = label(root_, "", 64, 103, 340, &lv_font_montserrat_14, muted());
        list_ = container(root_, 82, 126, 304, 260);
        lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_pad_right(list_, 8, 0);
        lv_obj_set_style_pad_bottom(list_, 4, 0);
        lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_, 8, 0);

        for (size_t i = 0; i < badge_schedule::Items.size(); ++i) {
            const auto& item = badge_schedule::Items[i];
            auto& row = rows_[i];
            row.box = container(list_, 0, 0, RowWidth, 1);
            lv_obj_set_height(row.box, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(row.box, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_all(row.box, 12, 0);
            lv_obj_set_style_pad_row(row.box, 8, 0);
            lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row.box, 10, 0);
            // Reserve the border and status line in every state: time updates
            // must never change row heights or move a user's scroll position.
            lv_obj_set_style_border_width(row.box, 2, 0);
            lv_obj_set_style_border_color(row.box, accent(), 0);
            auto* header = container(row.box, 0, 0, ContentWidth, 19);
            auto* time = label(header, item.time, 0, 0, 154, &lv_font_montserrat_14, muted());
            lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_LEFT, 0);
            row.now = label(header, "", 154, 0, ContentWidth - 154, &lv_font_montserrat_14, accent());
            lv_obj_set_style_text_align(row.now, LV_TEXT_ALIGN_RIGHT, 0);
            wrapped_label(row.box, item.title, &lv_font_montserrat_24, white());
            if (item.detail && item.detail[0]) wrapped_label(row.box, item.detail, &lv_font_montserrat_14, muted());
        }
        label(root_, "Swipe up / down", 84, 390, 300, &lv_font_montserrat_14, muted());
        update();
        lv_obj_update_layout(list_);
        const int current = context_.model.schedule_current;
        if (current >= 0 && current < int(rows_.size()) && rows_[current].state == badge_schedule::State::OnNow) {
            lv_obj_scroll_to_view(rows_[current].box, LV_ANIM_OFF);
        }
    }

    void update() override {
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
            lv_obj_set_style_bg_color(row.box, current ? lv_color_hex(0x292335) : panel(), 0);
            lv_obj_set_style_opa(row.box, state == badge_schedule::State::Passed ? LV_OPA_50 : LV_OPA_COVER, 0);
        }
    }
private:
    int minute_ = -2;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* subtitle_ = nullptr;
    std::array<ScheduleRow, badge_schedule::Items.size()> rows_{};
};
std::unique_ptr<PageView> make_schedule(Context& c, lv_obj_t* p) { return std::make_unique<SchedulePage>(c, p); }
} // namespace badge::ui
