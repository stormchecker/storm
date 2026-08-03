#pragma once

namespace storm {
namespace gspn {
struct LayoutInfo {
    LayoutInfo() {}
    LayoutInfo(double x, double y, double rotation = 0.0) : x(x), y(y), rotation(rotation) {}

    // x location
    double x = 0.0;
    // y location
    double y = 0.0;
    // degrees
    double rotation = 0.0;
};
}  // namespace gspn
}  // namespace storm