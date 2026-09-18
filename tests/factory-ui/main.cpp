#include "badge_ui.h"
#include "schedule.h"
#include "ui/design_fonts.h"
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
void assert_chrome(bool intro = false, bool filled_profile = false) {
    if (filled_profile) {
        assert(lit_pixels(10, 210, 64, 250) == 0);
        assert(lit_pixels(184, 430, 284, 442) == 0);
        return;
    }
    if (!intro) assert(lit_pixels(203, 24, 265, 38) > 30); // Supplied small mark.
    assert(lit_pixels(10, 210, 64, 253) > 40);
    assert(lit_pixels(404, 210, 458, 253) > 40);
    assert(lit_pixels(184, 430, 284, 442) > 25); // Square page indicators.
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
int visible_dots(lv_obj_t* object) {
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return 0;
    if ((lv_obj_get_width(object) == 8 && lv_obj_get_height(object) == 8) ||
        (lv_obj_get_width(object) == 12 && lv_obj_get_height(object) == 12)) return 1;
    int count = 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(object); ++i)
        count += visible_dots(lv_obj_get_child(object, i));
    return count;
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
    int bookmarked = -1, resets = 0;
    badge::UiCallbacks callbacks;
    callbacks.brightness = [&](int value) { brightness = value; };
    callbacks.orientation = [&](badge::Orientation value) { orientation = int(value); };
    callbacks.bookmark = [&](int index) { bookmarked = index; };
    callbacks.reset_badge = [&] { ++resets; };
    callbacks.network = [&](int value) { network = value; };
    callbacks.request_setup = [&] { ++setup; badge::ui_show_setup("init-test-badge", "example1234"); };
    callbacks.close_setup = [] { badge::ui_close_setup(false); };
    badge::ui_init(display, callbacks);
    badge::UiModel model;
    model.clock_text = "12:34 PM";
    model.date_text = "2026-09-17 12:34 PM";
    model.clock_valid = true;
    model.battery_percent = 74;
    badge::ui_update(model);
    spin();

    // A locked invitation is absent from every navigation route and the dots.
    assert(badge::ui_page_count() == 5);
    assert(visible_dots(lv_display_get_layer_top(display)) == 5);
    badge::ui_open_after_dark();
    assert(badge::ui_page_index() == 0);
    for (int id : {1, 3, 4, 5, 0}) {
        tap(434, 233);
        assert(badge::ui_page_index() == id);
    }
    for (int id : {5, 4, 3, 1, 0}) {
        badge::ui_button(false, -1); spin();
        assert(badge::ui_page_index() == id);
    }
    badge::ui_page(1); spin();
    swipe(320, 390, 150, 390);
    assert(badge::ui_page_index() == 3);
    badge::ui_page(-1); spin();
    assert(badge::ui_page_index() == 1);

    // A normal press-locked page contact may end over the separate chrome
    // layer: it pages once on release and cannot affect the next plain tap.
    badge::ui_page(-1); spin();
    touch(100, 390, true);
    touch(250, 390, true);
    touch(434, 200, true);
    touch(434, 270, true);
    touch(434, 270, false);
    assert(badge::ui_page_index() == 5);
    tap(234, 80);
    assert(badge::ui_page_index() == 5);
    tap(434, 233); // A fresh arrow contact still completes normally.
    assert(badge::ui_page_index() == 0);

    // Framework cancellation sends PRESS_LOST instead of RELEASED. An already
    // recognized gesture must be discarded even if release lands on chrome.
    touch(100, 390, true);
    touch(250, 390, true);
    lv_indev_wait_release(input);
    touch(434, 233, true);
    touch(434, 233, false);
    assert(badge::ui_page_index() == 0);
    tap(234, 80);
    assert(badge::ui_page_index() == 0);
    tap(434, 233);
    assert(badge::ui_page_index() == 1);

    // A clock reveal adds the dot/page without rebuilding the reader's agenda.
    auto* locked_title = find_label(lv_display_get_screen_active(display), badge_schedule::Items[0].title);
    auto* locked_schedule = lv_obj_get_parent(lv_obj_get_parent(locked_title));
    swipe(234, 335, 234, 170);
    const int locked_scroll = lv_obj_get_scroll_y(locked_schedule);
    assert(locked_scroll > 0);
    model.after_dark_unlocked = true;
    badge::ui_update(model); spin();
    assert(badge::ui_page_index() == 1 && badge::ui_page_count() == 6);
    assert(visible_dots(lv_display_get_layer_top(display)) == 6);
    assert(locked_title == find_label(lv_display_get_screen_active(display), badge_schedule::Items[0].title));
    assert(lv_obj_get_scroll_y(locked_schedule) == locked_scroll);
    badge::ui_open_after_dark(); spin();
    assert(badge::ui_page_index() == 2);
    assert(find_label(lv_display_get_screen_active(display), "After Dark"));
    snapshot("after-dark-unlocked");
    badge::ui_page(-2); spin();

    // A reveal during setup or Touch test cannot replace the modal. Stable IDs
    // preserve Settings returns even when the visible page count changes.
    model.after_dark_unlocked = false;
    badge::ui_update(model);
    badge::ui_page(-1); spin();
    badge::ui_show_setup("init-test-badge", "example1234"); spin();
    model.after_dark_unlocked = true;
    badge::ui_update(model);
    badge::ui_open_after_dark(); spin();
    assert(badge::ui_setup_active() && badge::ui_page_index() == 5);
    badge::ui_close_setup(true); spin();
    assert(badge::ui_page_index() == 5);
    badge::ui_show_touch_test();
    badge::ui_open_after_dark(); spin();
    assert(badge::ui_touch_test_active());
    badge::ui_close_touch_test();
    badge::ui_page(1); spin();

    for (int page = 0; page < 6; ++page) {
        assert(badge::ui_page_index() == page);
        snapshot("page" + std::to_string(page));
        assert_chrome(page == 0);
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
    assert(brightness == 70);
    tap(350, 238);
    assert(orientation == 2);

    auto* shown_date = find_label(lv_display_get_screen_active(display), "2026-09-17 | 12:34 PM");
    assert(shown_date);
    assert_inside(shown_date, lv_obj_get_parent(shown_date));
    // Reset is a separate confirmation, never a Settings-row side effect.
    tap(300, 404);
    assert(badge::ui_reset_active() && resets == 0);
    snapshot("reset-confirmation");
    badge::ui_page(1); badge::ui_open_after_dark(); spin();
    swipe(340, 245, 120, 245);
    assert(badge::ui_reset_active() && badge::ui_page_index() == 5);
    touch(234, 316, true); touch(234, 240, true); touch(234, 316, true); touch(234, 316, false);
    touch(234, 316, true, 40); touch(234, 316, false);
    assert(resets == 0);
    tap(234, 374);
    assert(!badge::ui_reset_active() && resets == 0);
    tap(300, 404);
    touch(234, 316, true);
    assert(resets == 0); // Press alone cannot erase anything.
    touch(234, 316, false);
    assert(resets == 1 && badge::ui_reset_active());
    badge::ui_button(true, 1); badge::ui_button(false, 1); spin();
    tap(234, 316);
    assert(badge::ui_reset_active() && resets == 1 && setup == 0);
    model.reset_state = badge::ResetState::Failed;
    badge::ui_update(model); spin();
    assert(find_label(lv_display_get_screen_active(display), "Retry"));
    snapshot("reset-failed");
    tap(234, 316);
    assert(resets == 2);
    model.reset_state = badge::ResetState::SettingsFailed;
    badge::ui_update(model); spin();
    assert(find_label(lv_display_get_screen_active(display), "Your profile was cleared. Some settings may have changed.\n\nTap Retry to finish the reset."));
    snapshot("reset-settings-failed");
    tap(234, 316);
    assert(resets == 3);
    model.reset_state = badge::ResetState::Complete;
    badge::ui_update(model); spin();
    snapshot("reset-complete");
    tap(234, 374);
    assert(!badge::ui_reset_active() && badge::ui_page_index() == 3);
    badge::ui_page(2); spin();
    // Reopening is always a new confirmation even after a successful reset.
    tap(300, 404);
    assert(find_label(lv_display_get_screen_active(display), "Reset badge?"));
    badge::ui_button(false, -1); spin();
    assert(!badge::ui_reset_active() && badge::ui_page_index() == 5 && resets == 3);

    tap(168, 404);
    assert(badge::ui_touch_test_active());
    badge::ui_touch_sample(234, 234, 234, 234, true, 0);
    snapshot("touch");
    assert(badge::ui_touch_state().x == 234);
    tap(234, 417);
    assert(!badge::ui_touch_test_active() && badge::ui_page_index() == 5);
    tap(234, 360);
    assert(badge::ui_setup_active() && setup == 1);
    snapshot("setup");
    tap(234, 414);
    assert(!badge::ui_setup_active() && badge::ui_page_index() == 5);
    assert_chrome();

    // Opening the real agenda focuses the active session once. Past sessions
    // dim, upcoming sessions stay readable, and full titles/details wrap.
    model.clock_text = "9:45 AM";
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
    assert(lv_obj_get_height(long_title) > font_sans_20.line_height);
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
    lv_area_t current_area;
    lv_obj_get_coords(current, &current_area);
    const int bookmark_y = current_area.y1 + 26;
    tap(340, bookmark_y);
    assert(bookmarked == 1);
    const int bookmarked_scroll = lv_obj_get_scroll_y(schedule);
    model.schedule_bookmarks = 1 << 1;
    badge::ui_update(model); spin();
    assert(lv_obj_get_scroll_y(schedule) == bookmarked_scroll);
    snapshot("schedule-bookmarked");
    bookmarked = -1;
    touch(340, bookmark_y, true);
    touch(310, bookmark_y, true);
    touch(340, bookmark_y, true);
    touch(340, bookmark_y, false);
    assert(bookmarked == -1);

    assert_idle();
    const int entry_scroll = lv_obj_get_scroll_y(schedule);
    swipe(234, 335, 234, 170);
    assert(lv_obj_get_scroll_y(schedule) > entry_scroll && badge::ui_page_index() == 1);
    const int user_scroll = lv_obj_get_scroll_y(schedule);
    snapshot("schedule-scrolled");

    // A new session updates existing widgets without moving the attendee's
    // chosen scroll position or changing row heights.
    model.clock_text = "11:02 AM";
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
    model.clock_text = "11:59 PM";
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
    model.clock_text = "12:00 AM";
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
    model.company = "WorkOS";
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
    assert_chrome(false, true);
    assert_idle();
    tap(234, 314);
    snapshot("expanded");
    assert_chrome(false, true);
    assert(lit_pixels(101, 104, 367, 370) > 20000); // Expanded QR is rendered.
    tap(234, 230);
    snapshot("profile-restored");
    assert_chrome(false, true);
    assert_idle();

    // Programmatic selection goes to the third, unconfigured social card.
    model.selected_network = 2;
    badge::ui_update(model);
    spin();
    tap(234, 280);
    assert(find_label(lv_display_get_screen_active(display), "No account yet"));
    tap(234, 300);
    assert(badge::ui_setup_active() && setup == 2);
    badge::ui_close_setup(true);
    spin();
    assert(badge::ui_page_index() == 3 && !badge::ui_setup_active());
    assert_chrome(false, true);

    // A profile with only a social URL must still expose that QR, and an
    // image replacement must not leave an expanded code for stale profile data.
    model.name.clear(); model.company.clear(); model.avatar = nullptr;
    model.avatar_width = model.avatar_height = 0;
    model.selected_network = 0; ++model.profile_revision;
    badge::ui_update(model); spin();
    tap(234, 180); spin();
    assert(find_label(lv_display_get_screen_active(display), "Tap to close"));
    assert(lit_pixels(101, 104, 367, 370) > 20000);
    model.company = "Example company"; ++model.profile_revision;
    badge::ui_update(model); spin();
    assert(!find_label(lv_display_get_screen_active(display), "Tap to close"));
    assert(find_label(lv_display_get_screen_active(display), "Example company"));

    mooncake::GetMooncake().uninstallAllApps();
    lv_indev_delete(input);
    lv_display_delete(display);
    lv_deinit();
    std::puts("PASS: native UI pages, partial rendering, idle efficiency, tap cancellation, daily schedule, scrolling, profiles and modal lifecycle");
}
