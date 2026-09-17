#include "board.h"
#include "board_rotation.h"
#include <cassert>
#include <iostream>

int main() {
    lv_init();
    auto* display = lv_display_create(board::NativeWidth, board::NativeHeight);
    assert(display);
    size_t checked = 0;
    for (uint8_t rotation = 0; rotation < 4; ++rotation) {
        lv_display_set_rotation(display, board::lvRotation(rotation));
        const int width = lv_display_get_horizontal_resolution(display);
        const int height = lv_display_get_vertical_resolution(display);
        // Check every pixel, including all four edges, of a non-square screen.
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                auto point = board::displayToNative(x, y, rotation, board::NativeWidth, board::NativeHeight);
                assert(point.x >= 0 && point.x < board::NativeWidth);
                assert(point.y >= 0 && point.y < board::NativeHeight);
                // Run LVGL's actual production implementation; do not replicate
                // its forward transform in the test or substitute a fake.
                lv_display_rotate_point(display, &point);
                assert(point.x == x && point.y == y);
                ++checked;
            }
        }
        // Controller coordinates outside the visible rectangle stay outside;
        // the transport must not clamp erroneous input onto a clickable edge.
        for (auto original : {lv_point_t{-10, 20}, lv_point_t{width + 5, height - 1}}) {
            auto point = board::displayToNative(original.x, original.y, rotation, board::NativeWidth, board::NativeHeight);
            lv_display_rotate_point(display, &point);
            assert(point.x == original.x && point.y == original.y);
        }
    }
    lv_display_delete(display);
    lv_deinit();
    std::cout << checked << " production adapter/LVGL rotation round trips passed\n";
}
