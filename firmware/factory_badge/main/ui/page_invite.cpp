#include "widgets.h"

namespace badge::ui {
class InvitePage final : public PageView {
public:
    InvitePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        label(root_, "Developers", 54, 78, 360, &font_sans_32, cream());
        label(root_, "After Dark", 54, 120, 360, &font_sans_32, cream());
        // The supplied mockup contains the customization URL, not an invite.
        // Keep this unscannable until a real invitation destination is supplied.
        auto* reserved = container(root_, 156, 195, 156, 156);
        lv_obj_set_style_bg_color(reserved, panel(), 0);
        lv_obj_set_style_bg_opa(reserved, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(reserved, muted(), 0);
        lv_obj_set_style_border_width(reserved, 1, 0);
        label(reserved, "Coming soon", 8, 69, 140, &font_mono_12, muted());
        label(root_, "Invite details coming soon", 64, 367, 340, &font_mono_semibold_12, muted());
    }
};
std::unique_ptr<PageView> make_after_dark(Context& c, lv_obj_t* p) { return std::make_unique<InvitePage>(c, p); }
} // namespace badge::ui
