#include "board_flush.h"
#include "lgfx/v1/panel/Panel_FrameBufferBase.hpp"
#include "lgfx/v1/platforms/common.hpp"
#include "lgfx/v1/misc/pixelcopy.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

// Only electrical operations are stubbed; dirty rectangle tracking, pixel
// copying, color conversion, and rotation use the pinned production driver.
namespace lgfx { inline namespace v1 {
void delay(unsigned long) {}
void gpio_hi(uint32_t) {}
void gpio_lo(uint32_t) {}
void pinMode(int_fast16_t, pin_mode_t) {}
} }

class MemoryPanel final : public lgfx::Panel_FrameBufferBase {
public:
    static constexpr int Width = 468, Height = 466;
    std::vector<uint16_t> pixels = std::vector<uint16_t>(Width * Height);
    std::array<uint8_t*, Height> rows{};
    int calls = 0, transactionDepth = 0;
    MemoryPanel(int rotation) {
        auto cfg = config();
        cfg.memory_width = cfg.panel_width = Width;
        cfg.memory_height = cfg.panel_height = Height;
        config(cfg);
        for (int y = 0; y < Height; ++y)
            rows[y] = reinterpret_cast<uint8_t*>(pixels.data() + y * Width);
        _lines_buffer = rows.data();
        setColorDepth(lgfx::rgb565_2Byte);
        setRotation(rotation);
        _range_mod.left = _range_mod.top = INT16_MAX;
        _range_mod.right = _range_mod.bottom = 0;
    }
    void startWrite() { assert(transactionDepth++ == 0); }
    void endWrite() { assert(--transactionDepth == 0); }
    void pushImage(int x, int y, int w, int h, const lgfx::rgb565_t* source) {
        assert(transactionDepth == 1);
        assert(w > 0 && h > 0 && w * h <= 8192);
        assert(x >= 0 && y >= 0 && x + w <= width() && y + h <= height());
        lgfx::pixelcopy_t copy(source, lgfx::rgb565_2Byte, lgfx::rgb565_nonswapped);
        copy.src_bitwidth = w;
        writeImage(x, y, w, h, &copy, false);
        ++calls;
    }
    const lgfx::range_rect_t& dirty() const { return _range_mod; }
};

void checkRegion(int rotation, int x, int y, int w, int h) {
    MemoryPanel panel(rotation);
    std::vector<lgfx::rgb565_t> source(w * h);
    for (int i = 0; i < w * h; ++i) source[i].raw = 1 + (i % 65534);
    board::flushImageChunks(panel, x, y, w, h, source.data());
    assert(panel.transactionDepth == 0 && panel.calls > 0);
    const auto dirty = panel.dirty();
    assert(!dirty.empty());

    // The tracked update must cover exactly the pixels the real driver changed,
    // including partial edge rectangles. No rotation formula is reproduced here.
    int changed = 0;
    for (int yy = 0; yy < MemoryPanel::Height; ++yy) {
        for (int xx = 0; xx < MemoryPanel::Width; ++xx) {
            const bool within = xx >= dirty.left && xx <= dirty.right &&
                                yy >= dirty.top && yy <= dirty.bottom;
            const bool written = panel.pixels[yy * MemoryPanel::Width + xx] != 0;
            assert(within == written);
            changed += written;
        }
    }
    assert(changed == w * h);

    // Read through the same driver's inverse path to check row continuity and
    // RGB565 byte order, which a dirty-bounds-only check would miss.
    std::vector<lgfx::rgb565_t> restored(w * h);
    lgfx::pixelcopy_t read(nullptr, lgfx::rgb565_nonswapped, lgfx::rgb565_2Byte);
    panel.readRect(x, y, w, h, restored.data(), &read);
    for (int i = 0; i < w * h; ++i) assert(restored[i].raw == source[i].raw);
}

int main() {
    // Prove the regression: old streaming writes change RAM but leave the
    // rotated dirty rectangle empty, so Panel_AMOLED::display() returns early.
    MemoryPanel old(2);
    std::vector<lgfx::rgb565_t> source(468 * 17);
    for (auto& pixel : source) pixel.raw = 0x1234;
    old.setWindow(0, 0, 467, 16);
    lgfx::pixelcopy_t copy(source.data(), lgfx::rgb565_2Byte, lgfx::rgb565_nonswapped);
    old.writePixels(&copy, source.size(), false);
    assert(old.dirty().empty());
    assert(std::count_if(old.pixels.begin(), old.pixels.end(), [](uint16_t p) { return p != 0; }) == 468 * 17);

    for (int rotation = 0; rotation < 4; ++rotation) {
        MemoryPanel geometry(rotation);
        const int w = geometry.width(), h = geometry.height();
        checkRegion(rotation, 0, 0, w, h);
        checkRegion(rotation, 0, 0, 17, 39);
        checkRegion(rotation, w - 33, h - 57, 33, 57);
        checkRegion(rotation, 11, 23, w - 19, 42);
        checkRegion(rotation, w - 1, h - 1, 1, 1);
    }
    std::puts("Pinned M5GFX old flush reproduced; production chunked flush passes all four orientations.");
}
