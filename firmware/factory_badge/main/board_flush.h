// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>

namespace board {
// The pinned framebuffer's writePixels() records reversed dirty bounds under
// rotation. pushImage()/writeImage() normalizes that rectangle. Keep each image
// below the factory's 8K-pixel copy bound and preserve complete source rows.
template<class Display, class Pixel>
void flushImageChunks(Display& display, int x, int y, int width, int height, const Pixel* source) {
    const int rowsPerChunk = std::max(1, 8192 / width);
    display.startWrite();
    for (int row = 0; row < height;) {
        const int rows = std::min(height - row, rowsPerChunk);
        display.pushImage(x, y + row, width, rows, source + row * width);
        row += rows;
    }
    display.endWrite();
}
}
