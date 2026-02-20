#pragma once
#include <array>
namespace vkmini {

struct Mat4 { std::array<float,16> m{}; };

Mat4 identity();
Mat4 perspective(float fovyRadians, float aspect, float zn, float zf);
Mat4 translate(float x, float y, float z);
Mat4 rotate_x(float r);
Mat4 rotate_y(float r);
Mat4 mul(const Mat4& a, const Mat4& b);

} // namespace vkmini
