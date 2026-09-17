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
        clock_ = label(root_, "", 144, 29, 180, &lv_font_montserrat_18, muted());
        label(root_, "Connect phone", 64, 66, 340, &lv_font_montserrat_24);
        label(root_, "Scan to join this badge's Wi-Fi", 54, 104, 360, &lv_font_montserrat_14, muted());
        code_frame_ = container(root_, 140, 129, 188, 188);
        ssid_ = label(root_, "Starting setup...", 54, 319, 360, &lv_font_montserrat_18);
        password_ = label(root_, "", 44, 345, 380, &lv_font_montserrat_14, muted());
        status_ = label(root_, "", 54, 366, 360, &lv_font_montserrat_14, muted());
        button(root_, "Back", 152, 392, 164, 42, [this] {
            if (context_.callbacks.close_setup) context_.callbacks.close_setup();
        });
        update();
    }
    void update() override {
        set_text(clock_, context_.model.clock_text);
        set_text(ssid_, context_.ssid.empty() ? "Starting setup..." : context_.ssid);
        set_text(password_, context_.password.empty() ? "" : "Password: " + context_.password);
        set_text(status_, context_.setup_status.empty() ? "Open http://" + context_.ip : context_.setup_status);
        const std::string payload = context_.ssid.empty() ? "" : "WIFI:T:WPA;S:" + wifi_escape(context_.ssid) + ";P:" + wifi_escape(context_.password) + ";;";
        if (payload != payload_) {
            payload_ = payload;
            lv_obj_clean(code_frame_);
            if (!payload.empty()) qr(code_frame_, payload, 0, 0, 188);
        }
    }
private:
    lv_obj_t *clock_ = nullptr, *ssid_ = nullptr, *password_ = nullptr, *status_ = nullptr, *code_frame_ = nullptr;
    std::string payload_;
};
std::unique_ptr<PageView> make_setup(Context& c, lv_obj_t* p) { return std::make_unique<SetupView>(c, p); }
} // namespace badge::ui
