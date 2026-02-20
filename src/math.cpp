#include "math.hpp"
#include <cmath>

namespace vkmini {

Mat4 identity()
{
    Mat4 r{};
    r.m = {1,0,0,0,
           0,1,0,0,
           0,0,1,0,
           0,0,0,1};
    return r;
}

Mat4 perspective(float fovyRadians, float aspect, float zn, float zf)
{
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    Mat4 r{};
    r.m = { f/aspect,0,0,0,
            0,f,0,0,
            0,0,(zf)/(zn-zf),-1.0f,
            0,0,(zn*zf)/(zn-zf),0 };
    return r;
}

Mat4 translate(float x, float y, float z)
{
    Mat4 r = identity();
    r.m[12]=x; r.m[13]=y; r.m[14]=z;
    return r;
}

Mat4 rotate_x(float rads)
{
    Mat4 r = identity();
    float c=std::cos(rads), s=std::sin(rads);
    r.m[5]=c; r.m[6]=s;
    r.m[9]=-s; r.m[10]=c;
    return r;
}
Mat4 rotate_y(float rads)
{
    Mat4 r = identity();
    float c=std::cos(rads), s=std::sin(rads);
    r.m[0]=c; r.m[2]=-s;
    r.m[8]=s; r.m[10]=c;
    return r;
}

Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 r{};
    for(int row=0;row<4;++row)
    for(int col=0;col<4;++col)
    {
        float v=0;
        for(int k=0;k<4;++k) v += a.m[row + k*4] * b.m[k + col*4];
        r.m[row + col*4] = v;
    }
    return r;
}

} // namespace vkmini
