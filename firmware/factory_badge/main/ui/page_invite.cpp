#include "widgets.h"

namespace badge::ui {
class InvitePage final : public PageView {
public:
    InvitePage(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        brand(root_, 73);
        label(root_, "Developers", 54, 131, 360, &lv_font_montserrat_36);
        label(root_, "After Dark", 54, 176, 360, &lv_font_montserrat_36);
        label(root_, "Invite details coming soon", 64, 230, 340, &lv_font_montserrat_18, muted());
        auto* reserved = container(root_, 174, 267, 120, 95);
        lv_obj_set_style_border_color(reserved, muted(), 0);
        lv_obj_set_style_border_width(reserved, 1, 0);
        lv_obj_set_style_radius(reserved, 12, 0);
        label(reserved, "QR", 0, 16, 120, &lv_font_montserrat_24, muted());
        label(reserved, "reserved", 0, 55, 120, &lv_font_montserrat_14, muted());
        label(root_, "Placeholder / no destination yet", 64, 387, 340, &lv_font_montserrat_14, muted());
    }
};
std::unique_ptr<PageView> make_after_dark(Context& c, lv_obj_t* p) { return std::make_unique<InvitePage>(c, p); }
} // namespace badge::ui
