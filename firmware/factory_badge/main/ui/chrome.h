#pragma once
#include "widgets.h"
#include <array>

namespace badge::ui {
class Chrome {
public:
    Chrome(Context& context, lv_obj_t* parent);
    ~Chrome();
    void update();
    void reflow();
private:
    Context& context_;
    lv_obj_t *root_ = nullptr, *clock_ = nullptr, *footer_ = nullptr;
    std::array<lv_obj_t*, PageCount> dots_{};
    int page_ = -1;
    int visible_count_ = -1;
};
} // namespace badge::ui
