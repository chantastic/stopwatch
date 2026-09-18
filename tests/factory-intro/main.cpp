#include <lvgl.h>
#include "intro_loop.h"
#include "reference.h"
#include <cassert>
#include <cstdio>

namespace {
uint16_t buffer[468 * 40];

unsigned timers() {
    unsigned count = 0;
    for (auto* timer = lv_timer_get_next(nullptr); timer; timer = lv_timer_get_next(timer)) ++count;
    return count;
}

uint64_t fingerprint(lv_obj_t* gif) {
    const auto* frame = static_cast<const lv_draw_buf_t*>(lv_image_get_src(gif));
    assert(frame && frame->header.cf == LV_COLOR_FORMAT_RGB565);
    uint64_t value = 14695981039346656037ULL;
    for (unsigned y = 0; y < frame->header.h; ++y) {
        const auto* row = reinterpret_cast<const uint16_t*>(frame->data + y * frame->header.stride);
        for (unsigned x = 0; x < frame->header.w; ++x) value = (value ^ row[x]) * 1099511628211ULL;
    }
    return value;
}

void advance(unsigned milliseconds) {
    // Match source millisecond deadlines without a synthetic polling delay.
    for (unsigned i = 0; i < milliseconds; ++i) { lv_tick_inc(1); lv_timer_handler(); }
}

template<size_t N>
void check(const lv_image_dsc_t& source, const uint64_t (&expected)[N], const uint16_t (&delays)[N]) {
    const auto timer_count = timers();
    // Whole-parent deletion matches PageView teardown during paging/modal entry.
    for (unsigned visit = 0; visit < 3; ++visit) {
        auto* page = lv_obj_create(lv_screen_active());
        auto* gif = lv_gif_create(page);
        lv_gif_set_color_format(gif, LV_COLOR_FORMAT_RGB565);
        lv_gif_set_src(gif, &source);
        assert(lv_gif_is_loaded(gif));
        assert(lv_gif_get_frame_count(gif) == N);
        // The pinned library's frame-count query scans/seeks the source.
        // Reload before timing playback; application code never uses that query.
        lv_gif_set_src(gif, &source);
        assert(lv_gif_get_loop_count(gif) == 0);
        assert(timers() == timer_count + 1);
        for (unsigned loop = 0; loop < 2; ++loop) {
            for (unsigned frame = 0; frame < N; ++frame) {
                const auto actual = fingerprint(gif);
                if (actual != expected[frame]) {
                    std::fprintf(stderr, "GIF %zu frames, visit %u, loop %u, frame %u: got %016llx expected %016llx\n",
                                 N, visit, loop, frame, static_cast<unsigned long long>(actual),
                                 static_cast<unsigned long long>(expected[frame]));
                    assert(false);
                }
                advance(delays[frame]);
            }
        }
        lv_obj_delete(page);
        assert(timers() == timer_count);
        advance(200); // A deleted GIF must not retain a pending callback.
    }
}
}

int main() {
    lv_init();
    auto* display = lv_display_create(468, 466);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, [](lv_display_t* display, const lv_area_t*, uint8_t*) { lv_display_flush_ready(display); });
    check(intro_loop, IntroFrames, IntroDelays);
    lv_image_dsc_t disposal = {};
    disposal.header.magic = LV_IMAGE_HEADER_MAGIC;
    disposal.header.cf = LV_COLOR_FORMAT_RAW;
    disposal.header.w = disposal.header.h = 4;
    disposal.data = DisposalGif;
    disposal.data_size = sizeof(DisposalGif);
    check(disposal, DisposalFrames, DisposalDelays);
    lv_display_delete(display);
    lv_deinit();
    std::puts("Native GIF: all 60 source frames/timing match Pillow, repeat and restore-background frames match; six parent deletion cycles release timers");
}
