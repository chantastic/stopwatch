#include "widgets.h"

namespace badge::ui {
static std::string wifi_escape(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        if (ch == '\\' || ch == ';' || ch == ',' || ch == ':' || ch == '"') escaped += '\\';
        escaped += ch;
    }
    return escaped;
}
class SetupView final : public PageView {
public:
    SetupView(Context& context, lv_obj_t* parent) : PageView(context, parent) {
        brand(root_, 24);
        label(root_, "Connect phone", 64, 64, 340, &font_sans_24);
        label(root_, "Scan to join this badge's Wi-Fi", 54, 98, 360, &font_mono_semibold_12, muted());
        code_frame_ = container(root_, 153, 135, 162, 162);
        ssid_ = label(root_, "Starting setup...", 54, 317, 360, &font_mono_semibold_12);
        password_ = label(root_, "", 44, 339, 380, &font_mono_semibold_12, muted());
        status_ = label(root_, "", 54, 360, 360, &font_mono_semibold_12, muted());
        button(root_, "Go back", 182, 396, 104, 44, [this] {
            if (context_.callbacks.close_setup) context_.callbacks.close_setup();
        });
        update();
    }
    void update() override {
        set_text(ssid_, context_.ssid.empty() ? "Starting setup..." : context_.ssid);
        set_text(password_, context_.password.empty() ? "" : "Password: " + context_.password);
        set_text(status_, context_.setup_status.empty() ? "Open http://" + context_.ip : context_.setup_status);
        const std::string payload = context_.ssid.empty() ? "" : "WIFI:T:WPA;S:" + wifi_escape(context_.ssid) + ";P:" + wifi_escape(context_.password) + ";;";
        if (payload != payload_) {
            payload_ = payload;
            lv_obj_clean(code_frame_);
            if (!payload.empty()) qr(code_frame_, payload, 0, 0, 162);
        }
    }
private:
    lv_obj_t *ssid_ = nullptr, *password_ = nullptr, *status_ = nullptr, *code_frame_ = nullptr;
    std::string payload_;
};
std::unique_ptr<PageView> make_setup(Context& c, lv_obj_t* p) { return std::make_unique<SetupView>(c, p); }
} // namespace badge::ui
