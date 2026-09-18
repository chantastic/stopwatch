#include "widgets.h"
#include "design_assets.h"
#include <string_view>

namespace badge::ui {
static_assert(std::string_view(HackUrl) == supplied_hack_qr_url,
              "The supplied Hack QR must match the current destination.");
class HackPage final : public PageView {
public:
    HackPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Hack this device.", 64, 64, 340, &font_sans_24, cream());
        label(root_, "Learn how", 54, 98, 360, &font_mono_semibold_12, muted());
        auto* code = lv_image_create(root_);
        lv_image_set_src(code, &supplied_hack_qr);
        lv_obj_set_pos(code, 123, 138);
        lv_obj_set_style_image_recolor(code, white(), 0);
        lv_obj_set_style_image_recolor_opa(code, LV_OPA_COVER, 0);
        label(root_, "drop.workos.cloud/stopwatch", 44, 383, 380, &font_mono_semibold_12, muted());
    }
};
std::unique_ptr<PageView> make_hack(Context& c, lv_obj_t* p) { return std::make_unique<HackPage>(c, p); }
} // namespace badge::ui
