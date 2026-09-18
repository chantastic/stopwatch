#include "widgets.h"

namespace badge::ui {
class HackPage final : public PageView {
public:
    HackPage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Make it yours", 64, 64, 340, &font_sans_24);
        label(root_, "Build something for your badge", 54, 98, 360, &font_mono_12, muted());
        qr(root_, HackUrl, 123, 138, 222);
        label(root_, "drop.workos.cloud/stopwatch", 44, 384, 380, &font_mono_12, muted());
    }
};
std::unique_ptr<PageView> make_hack(Context& c, lv_obj_t* p) { return std::make_unique<HackPage>(c, p); }
} // namespace badge::ui
