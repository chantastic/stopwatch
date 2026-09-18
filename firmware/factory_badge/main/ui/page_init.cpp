#include "widgets.h"
#include "intro_loop.h"

namespace badge::ui {
class InitPage final : public PageView {
public:
    InitPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        // LVGL owns decoding, source frame delays and looping. Its RGB565
        // framebuffer is bounded to 468 x 466; the embedded GIF stays in flash.
        auto* loop = lv_gif_create(root_);
        lv_gif_set_color_format(loop, LV_COLOR_FORMAT_RGB565);
        lv_gif_set_src(loop, &intro_loop);
        lv_obj_center(loop);
        lv_obj_remove_flag(loop, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(loop, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(loop, LV_OBJ_FLAG_EVENT_BUBBLE);
        // The PageView tree owns the decoder and its timer. Navigating away or
        // opening a modal deletes both through LVGL's normal object lifecycle.
        brand(root_, 210, true);
    }
};
std::unique_ptr<PageView> make_init(Context& c, lv_obj_t* p) { return std::make_unique<InitPage>(c, p); }
} // namespace badge::ui
