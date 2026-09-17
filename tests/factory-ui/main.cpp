#include "badge_ui.h"
#include "schedule.h"
#include <mooncake.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr int Width = 468, Height = 466;
std::vector<uint16_t> pixels(Width * Height);
uint16_t buffer[Width * 40];
uint32_t clock_ms = 0;
int touch_x = 0, touch_y = 0;
bool pressed = false;
unsigned flush_count = 0;
std::string output_directory;

void spin(int count = 6) {
    for (int i = 0; i < count; ++i) {
        clock_ms += 20;
        lv_tick_inc(20);
        badge::ui_tick(clock_ms);
        lv_timer_handler();
    }
}
void touch(int x, int y, bool down, int ticks = 3) {
    touch_x = x;
    touch_y = y;
    pressed = down;
    spin(ticks);
}
void tap(int x, int y) {
    touch(x, y, true, 5);
    touch(x, y, false, 6);
}
void swipe(int x0, int y0, int x1, int y1) {
    touch(x0, y0, true);
    for (int i = 1; i <= 12; ++i) touch(x0 + (x1 - x0) * i / 12, y0 + (y1 - y0) * i / 12, true, 1);
    touch(x1, y1, false);
    spin(60); // Allow LVGL's native scroll throw and snap to settle.
}
int lit_pixels(int x0, int y0, int x1, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
        const auto p = pixels[y * Width + x];
        if (((p >> 11) & 31) > 7) ++count;
    }
    return count;
}
void assert_chrome() {
    assert(lit_pixels(134, 26, 334, 47) > 100);  // Clock.
    assert(lit_pixels(10, 210, 64, 250) > 40);   // Left arrow.
    assert(lit_pixels(404, 210, 458, 250) > 40); // Right arrow.
    assert(lit_pixels(84, 416, 384, 438) > 150); // Page name.
    assert(lit_pixels(191, 445, 278, 452) > 25); // Page dots.
}
void assert_idle() {
    spin(30); // Finish button style transitions.
    const auto before = flush_count;
    spin(30);
    assert(flush_count == before); // Static pages do no periodic redraws.
}
void snapshot(const std::string& name) {
    spin();
    if (output_directory.empty()) return;
    auto path = output_directory + "/" + name + ".ppm";
    auto* file = std::fopen(path.c_str(), "wb");
    assert(file);
    std::fprintf(file, "P6\n%d %d\n255\n", Width, Height);
    for (auto p : pixels) {
        const unsigned char rgb[3] = {uint8_t(((p >> 11) & 31) * 255 / 31),
            uint8_t(((p >> 5) & 63) * 255 / 63), uint8_t((p & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    std::fclose(file);
}
lv_obj_t* find_label(lv_obj_t* object, const char* text) {
    if (lv_obj_check_type(object, &lv_label_class) && !std::strcmp(lv_label_get_text(object), text)) return object;
    for (unsigned i = 0; i < lv_obj_get_child_count(object); ++i) {
        if (auto* found = find_label(lv_obj_get_child(object, i), text)) return found;
    }
    return nullptr;
}
void assert_inside(lv_obj_t* child, lv_obj_t* parent) {
    lv_area_t child_area, parent_area;
    lv_obj_get_coords(child, &child_area);
    lv_obj_get_coords(parent, &parent_area);
    assert(child_area.x1 >= parent_area.x1 && child_area.x2 <= parent_area.x2);
    assert(child_area.y1 >= parent_area.y1 && child_area.y2 <= parent_area.y2);
}

}

int main(int argc, char** argv) {
    if (argc == 2) {
        output_directory = argv[1];
        std::filesystem::create_directories(output_directory);
    }
    lv_init();
    auto* display = lv_display_create(Width, Height);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t* area, uint8_t* map) {
        ++flush_count;
        auto* source = reinterpret_cast<uint16_t*>(map);
        const int stride = lv_display_get_buf_active(d)->header.stride / sizeof(uint16_t);
        assert(area->x1 >= 0 && area->x2 < Width && area->y1 >= 0 && area->y2 < Height);
        for (int y = area->y1; y <= area->y2; ++y) for (int x = area->x1; x <= area->x2; ++x) {
            pixels[y * Width + x] = source[(y - area->y1) * stride + x - area->x1];
        }
        lv_display_flush_ready(d);
    });
    auto* input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, [](lv_indev_t*, lv_indev_data_t* data) {
        data->point = {touch_x, touch_y};
        data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    });

    int brightness = 0, orientation = -1, network = -1, setup = 0;
    badge::UiCallbacks callbacks;
    callbacks.brightness = [&](int value) { brightness = value; };
    callbacks.orientation = [&](badge::Orientation value) { orientation = int(value); };
    callbacks.network = [&](int value) { network = value; };
    callbacks.request_setup = [&] { ++setup; badge::ui_show_setup("init-test-badge", "example1234"); };
    callbacks.close_setup = [] { badge::ui_close_setup(false); };
    badge::ui_init(display, callbacks);
    badge::UiModel model;
    model.clock_text = "12:34";
    model.date_text = "Sep 17, 2026 12:34";
    model.clock_valid = true;
    model.battery_percent = 74;
    badge::ui_update(model);
    spin();

    for (int page = 0; page < 6; ++page) {
        assert(badge::ui_page_index() == page);
        snapshot("page" + std::to_string(page));
        assert_chrome();
        if (page != 0) assert_idle();
        if (page < 5) {
            tap(434, 233); // Native arrow hit test, release and page lifecycle.
            assert(badge::ui_page_index() == page + 1);
        }
    }

    // Native SHORT_CLICKED alone accepts a drag that returns to the button.
    // The shared tap binding must cancel the entire gesture once it moves/loses focus.
    touch(342, 177, true);
    touch(342, 110, true);
    touch(342, 177, true);
    touch(342, 177, false, 6);
    assert(brightness == 0);
    // A shorter excursion stays inside the button but is still a drag.
    touch(330, 177, true);
    touch(354, 177, true);
    touch(330, 177, false, 6);
    assert(brightness == 0);
    touch(342, 177, true, 40);
    touch(342, 177, false, 6);
    assert(brightness == 0);
    tap(342, 177);
    assert(brightness == 60);
    tap(332, 369);
    assert(orientation == 2);

    tap(314, 283);
    assert(badge::ui_touch_test_active());
    badge::ui_touch_sample(234, 234, 234, 234, true, 0);
    snapshot("touch");
    assert(badge::ui_touch_state().x == 234);
    tap(234, 417);
    assert(!badge::ui_touch_test_active() && badge::ui_page_index() == 5);
    tap(154, 283);
    assert(badge::ui_setup_active() && setup == 1);
    snapshot("setup");
    tap(234, 414);
    assert(!badge::ui_setup_active() && badge::ui_page_index() == 5);
    assert_chrome();

    // Opening the real agenda focuses the active session once. Past sessions
    // dim, upcoming sessions stay readable, and full titles/details wrap.
    model.clock_text = "09:45";
    model.schedule_minute = 9 * 60 + 45;
    model.schedule_current = badge_schedule::current(model.schedule_minute);
    badge::ui_update(model);
    badge::ui_page(-4);
    spin();
    auto* first_title = find_label(lv_display_get_screen_active(display), badge_schedule::Items[0].title);
    assert(first_title);
    auto* schedule = lv_obj_get_parent(lv_obj_get_parent(first_title));
    assert(lv_obj_get_child_count(schedule) == badge_schedule::Items.size());
    std::array<int, badge_schedule::Items.size()> heights{};
    for (size_t i = 0; i < badge_schedule::Items.size(); ++i) {
        auto* row = lv_obj_get_child(schedule, i);
        const auto& item = badge_schedule::Items[i];
        auto* title = find_label(row, item.title);
        assert(title && find_label(row, item.time));
        assert(lv_label_get_long_mode(title) == LV_LABEL_LONG_WRAP);
        assert_inside(title, row);
        if (item.detail[0]) assert_inside(find_label(row, item.detail), row);
        heights[i] = lv_obj_get_height(row);
    }
    auto* long_title = find_label(schedule, "Networking break and sponsors");
    assert(lv_obj_get_height(long_title) > lv_font_montserrat_24.line_height);
    auto* past = lv_obj_get_child(schedule, 0);
    auto* current = lv_obj_get_child(schedule, 1);
    auto* upcoming = lv_obj_get_child(schedule, 2);
    assert(lv_obj_get_style_opa(past, LV_PART_MAIN) == LV_OPA_50);
    assert(lv_obj_get_style_opa(current, LV_PART_MAIN) == LV_OPA_COVER);
    assert(lv_obj_get_style_border_opa(current, LV_PART_MAIN) == LV_OPA_COVER);
    assert(find_label(current, "On now"));
    assert(lv_obj_get_style_border_opa(upcoming, LV_PART_MAIN) == LV_OPA_TRANSP);
    assert(lv_obj_get_style_opa(upcoming, LV_PART_MAIN) == LV_OPA_COVER);
    assert_inside(current, schedule);
    snapshot("schedule-current");
    assert_idle();
    const int entry_scroll = lv_obj_get_scroll_y(schedule);
    swipe(234, 335, 234, 170);
    assert(lv_obj_get_scroll_y(schedule) > entry_scroll && badge::ui_page_index() == 1);
    const int user_scroll = lv_obj_get_scroll_y(schedule);
    snapshot("schedule-scrolled");

    // A new session updates existing widgets without moving the attendee's
    // chosen scroll position or changing row heights.
    model.clock_text = "11:02";
    model.schedule_minute = 11 * 60 + 2;
    model.schedule_current = badge_schedule::current(model.schedule_minute);
    badge::ui_update(model);
    spin();
    assert(lv_obj_get_scroll_y(schedule) == user_scroll);
    assert(lv_obj_get_style_opa(current, LV_PART_MAIN) == LV_OPA_50);
    assert(find_label(upcoming, "On now"));
    for (size_t i = 0; i < heights.size(); ++i) assert(lv_obj_get_height(lv_obj_get_child(schedule, i)) == heights[i]);
    snapshot("schedule-passed");
    assert_idle();
    const auto settled_flush_count = flush_count;
    ++model.schedule_minute; // Same session: no schedule properties need redraw.
    badge::ui_update(model);
    spin();
    assert(flush_count == settled_flush_count);

    model.clock_text = "--:--";
    model.clock_valid = false;
    model.schedule_minute = -1;
    model.schedule_current = -1;
    badge::ui_update(model);
    spin();
    assert(find_label(lv_display_get_screen_active(display), "Set time in Settings"));
    assert(!find_label(schedule, "On now"));
    for (size_t i = 0; i < heights.size(); ++i) {
        auto* row = lv_obj_get_child(schedule, i);
        assert(lv_obj_get_style_opa(row, LV_PART_MAIN) == LV_OPA_COVER);
        assert(lv_obj_get_style_border_opa(row, LV_PART_MAIN) == LV_OPA_TRANSP);
    }
    assert(lv_obj_get_scroll_y(schedule) == user_scroll);
    snapshot("schedule-invalid-time");
    model.clock_valid = true;
    model.clock_text = "23:59";
    model.schedule_minute = 23 * 60 + 59;
    model.schedule_current = badge_schedule::current(model.schedule_minute);
    badge::ui_update(model);
    spin();
    assert(find_label(lv_obj_get_child(schedule, 8), "On now"));
    assert(find_label(lv_obj_get_child(schedule, 8), "End time not listed"));
    badge::ui_page(1); spin();
    badge::ui_page(-1); spin(); // Re-entry must make the late-day current row visible.
    auto* last_title = find_label(lv_display_get_screen_active(display), badge_schedule::Items[8].title);
    schedule = lv_obj_get_parent(lv_obj_get_parent(last_title));
    assert_inside(lv_obj_get_child(schedule, 8), schedule);
    const int last_entry_scroll = lv_obj_get_scroll_y(schedule);
    assert_chrome();
    snapshot("schedule-last-session");
    model.clock_text = "00:00";
    model.schedule_minute = 0; // The published agenda repeats on the next local day.
    model.schedule_current = -1;
    badge::ui_update(model);
    spin();
    assert(!find_label(schedule, "On now"));
    for (size_t i = 0; i < heights.size(); ++i) assert(lv_obj_get_style_opa(lv_obj_get_child(schedule, i), LV_PART_MAIN) == LV_OPA_COVER);
    assert(lv_obj_get_scroll_y(schedule) == last_entry_scroll);
    snapshot("schedule-midnight");
    assert_chrome();
    assert_idle();

    badge::ui_page(2);
    spin();
    swipe(234, 338, 234, 160);
    assert(network > 0 && badge::ui_page_index() == 3);
    // Populate each social independently; the current card must remain selected.
    model.name = "Conference attendee";
    model.socials[0] = "https://github.com/octocat";
    model.socials[1] = "https://x.com/example";
    model.selected_network = 0;
    model.profile_revision = 1;
    std::vector<uint16_t> avatar(160 * 160, 0xf800);
    model.avatar = avatar.data();
    model.avatar_width = model.avatar_height = 160;
    badge::ui_update(model);
    spin();
    snapshot("profile");
    assert_chrome();
    assert_idle();
    tap(234, 314);
    snapshot("expanded");
    assert_chrome();
    assert(lit_pixels(101, 104, 367, 370) > 20000); // Expanded QR is rendered.
    tap(234, 230);
    snapshot("profile-restored");
    assert_chrome();
    assert_idle();

    // Programmatic selection goes to the third, unconfigured social card.
    model.selected_network = 2;
    badge::ui_update(model);
    spin();
    tap(234, 339);
    assert(badge::ui_setup_active() && setup == 2);
    badge::ui_close_setup(true);
    spin();
    assert(badge::ui_page_index() == 3 && !badge::ui_setup_active());
    assert_chrome();

    mooncake::GetMooncake().uninstallAllApps();
    lv_indev_delete(input);
    lv_display_delete(display);
    lv_deinit();
    std::puts("PASS: native UI pages, partial rendering, idle efficiency, tap cancellation, daily schedule, scrolling, profiles and modal lifecycle");
}
