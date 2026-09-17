#include "chrome.h"

namespace badge::ui {
Chrome::Chrome(Context& context, lv_obj_t* parent) : context_(context) {
    root_ = container(parent, 0, 0, Width, Height);
    lv_obj_center(root_);
    // Transparent chrome must not intercept the page's controls or scrolling.
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    clock_ = label(root_, "--:--", 134, 26, 200, &lv_font_montserrat_18, muted());
    auto* left = button(root_, LV_SYMBOL_LEFT, 10, 170, 54, 126, [] { ui_page(-1); });
    auto* right = button(root_, LV_SYMBOL_RIGHT, 404, 170, 54, 126, [] { ui_page(1); });
    for (auto* arrow : {left, right}) {
        lv_obj_set_style_bg_opa(arrow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(arrow, LV_OPA_30, LV_STATE_PRESSED);
        lv_obj_set_style_radius(arrow, 24, 0);
    }
    footer_ = label(root_, "", 84, 416, 300, &lv_font_montserrat_18);
    for (int i = 0; i < PageCount; ++i) {
        dots_[i] = container(root_, 191 + i * 16, 445, 7, 7);
        lv_obj_remove_flag(dots_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots_[i], LV_OPA_COVER, 0);
    }
    update();
}
Chrome::~Chrome() { if (root_) lv_obj_delete(root_); }
void Chrome::reflow() { lv_obj_center(root_); }
void Chrome::update() {
    set_hidden(root_, context_.setup || context_.touch_test);
    set_text(clock_, context_.model.clock_text);
    set_text(footer_, context_.page == 5 && context_.model.settings_pending ? "Saving settings..." : PageNames[context_.page]);
    if (page_ != context_.page) {
        page_ = context_.page;
        for (int i = 0; i < PageCount; ++i) lv_obj_set_style_bg_color(dots_[i], i == page_ ? white() : lv_color_hex(0x55555e), 0);
    }
}
} // namespace badge::ui
