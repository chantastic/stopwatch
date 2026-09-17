#include "widgets.h"

namespace badge::ui {
class HackPage final : public PageView {
public:
    HackPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        brand(root_, 67);
        label(root_, "Make it yours.", 64, 112, 340, &lv_font_montserrat_36);
        label(root_, "Build something for your badge", 54, 162, 360, &lv_font_montserrat_18, muted());
        qr(root_, HackUrl, 138, 190, 192);
        label(root_, "drop.workos.cloud/stopwatch", 44, 389, 380, &lv_font_montserrat_14, muted());
    }
};
std::unique_ptr<PageView> make_hack(Context& c, lv_obj_t* p) { return std::make_unique<HackPage>(c, p); }
} // namespace badge::ui
