#include "util/AnypointRemap.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <cstring>

namespace dso {

namespace {
constexpr float PI               = 3.14159265358979323846f;
constexpr float PCT_UNIT_WIDTH   = 1.27f;
constexpr float PCT_UNIT_HEIGHT  = 1.27f;
constexpr float FOCAL_MODE1      = 410.0f;
constexpr float FOCAL_MODE2      = 455.9f;

inline float poly6(float a, float p0,float p1,float p2,float p3,float p4,float p5) {
    const float a2 = a*a, a3 = a2*a, a4 = a3*a, a5 = a4*a, a6 = a5*a;
    return p0*a6 + p1*a5 + p2*a4 + p3*a3 + p4*a2 + p5*a;
}

inline bool paramsEqual(const AnypointParams& a, const AnypointParams& b) {
    return std::memcmp(&a, &b, sizeof(AnypointParams)) == 0;
}
} // anon

void AnypointRemap::init(int outW, int outH, int fisheyeW, int fisheyeH) {
    outW_  = outW;
    outH_  = outH;
    fishW_ = fisheyeW;
    fishH_ = fisheyeH;
    mapX_.create(outH_, outW_, CV_32FC1);
    mapY_.create(outH_, outW_, CV_32FC1);
    cacheValid_ = false;
}

bool AnypointRemap::apply(const cv::Mat& src, cv::Mat& dst) {
    if (mapX_.empty()) return false;

    const AnypointParams p = getAnypointParams();
    if (!cacheValid_ || !paramsEqual(p, cached_)) {
        rebuildMap(p);
        cached_     = p;
        cacheValid_ = true;
    }

    if (!p.enabled) {
        // Bypass: just resize the fisheye to the output size so DSO sees a
        // consistent frame regardless of the toggle.
        if (src.cols == outW_ && src.rows == outH_) dst = src;
        else cv::resize(src, dst, cv::Size(outW_, outH_), 0, 0, cv::INTER_AREA);
        return true;
    }

    cv::remap(src, dst, mapX_, mapY_, cv::INTER_LINEAR,
              cv::BORDER_CONSTANT, cv::Scalar(0));
    return true;
}

void AnypointRemap::rebuildMap(const AnypointParams& p) {
    switch (p.mode) {
        case 0: fillMode1(p); fxVirtual_ = fyVirtual_ = (FOCAL_MODE1 / PCT_UNIT_WIDTH) * p.param3; break;
        case 1: fillMode2(p); fxVirtual_ = fyVirtual_ = (FOCAL_MODE2 / PCT_UNIT_WIDTH) * p.param3; break;
        case 2: fillTube(p);  fxVirtual_ = fyVirtual_ = outW_ * 0.5f; break;
        case 3: fillCar(p);   fxVirtual_ = fyVirtual_ = outW_ * 0.5f; break;
        default: fillMode1(p); break;
    }
}

void AnypointRemap::fillMode1(const AnypointParams& p) {
    const float imgW  = (float)fishW_;
    const float imgH  = (float)fishH_;
    const float dcx   = imgW * 0.5f * p.calibrationRatio;
    const float dcy   = imgH * 0.5f * p.calibrationRatio;
    const float icx   = p.iCx * p.calibrationRatio;
    const float icy   = p.iCy * p.calibrationRatio;

    const float mBeta  = (p.param2 + 180.0f) * (PI / 180.0f);
    const float mAlpha =  p.param1            * (PI / 180.0f);
    const float zoom   =  p.param3;

    const float wCosB    = PCT_UNIT_WIDTH  * std::cos(mBeta);
    const float hCosASB  = PCT_UNIT_HEIGHT * std::cos(mAlpha) * std::sin(mBeta);
    const float fZSASB   = FOCAL_MODE1 * zoom * std::sin(mAlpha) * std::sin(mBeta);
    const float wSinB    = PCT_UNIT_WIDTH  * std::sin(mBeta);
    const float hCosACB  = PCT_UNIT_HEIGHT * std::cos(mAlpha) * std::cos(mBeta);
    const float fZSACB   = FOCAL_MODE1 * zoom * std::sin(mAlpha) * std::cos(mBeta);
    const float hSinA    = PCT_UNIT_HEIGHT * std::sin(mAlpha);
    const float fZCosA   = FOCAL_MODE1 * zoom * std::cos(mAlpha);

    const float p0 = p.p[0], p1 = p.p[1], p2 = p.p[2],
                p3 = p.p[3], p4 = p.p[4], p5 = p.p[5];

    for (int y = 0; y < outH_; ++y) {
        float* mx = mapX_.ptr<float>(y);
        float* my = mapY_.ptr<float>(y);
        const float v = (float)y / (float)outH_;          // top-left origin
        for (int x = 0; x < outW_; ++x) {
            const float u = (float)x / (float)outW_;
            const float positionX = u * imgW;
            const float positionY = (1.0f - v) * imgH;     // matches shader

            const float tx = (positionX - dcx) * wCosB - (positionY - dcy) * hCosASB + fZSASB;
            const float ty = (positionX - dcx) * wSinB + (positionY - dcy) * hCosACB - fZSACB;
            const float tz = (positionY - dcy) * hSinA + fZCosA;

            const float alpha = std::atan2(std::sqrt(tx*tx + ty*ty), tz);
            const float beta  = (tx != 0.0f) ? std::atan2(ty, tx)
                                             : ((ty >= 0) ? PI*0.5f : -PI*0.5f);

            const float poly = poly6(alpha, p0,p1,p2,p3,p4,p5);
            const float senH = icx * p.calibrationRatio
                             - poly * p.calibrationRatio * p.calibrationRatio * std::cos(beta);
            const float senV = icy
                             - poly * p.calibrationRatio * p.calibrationRatio * std::sin(beta);

            const float sx = senH / p.calibrationRatio;
            const float sy = senV;

            if (sx >= 0 && sx < imgW && sy >= 0 && sy < imgH) {
                mx[x] = sx;
                my[x] = sy;
            } else {
                mx[x] = -1.0f;  // remap will sample border colour
                my[x] = -1.0f;
            }
        }
    }
}

void AnypointRemap::fillMode2(const AnypointParams& p) {
    const float imgW  = (float)fishW_;
    const float imgH  = (float)fishH_;
    const float dcx   = imgW * 0.5f * p.calibrationRatio;
    const float dcy   = imgH * 0.5f * p.calibrationRatio;
    const float icx   = p.iCx * p.calibrationRatio;
    const float icy   = p.iCy * p.calibrationRatio;

    const float thetaX = p.param1 * (PI / 180.0f);
    const float thetaY = p.param2 * (PI / 180.0f);
    const float zoom   = p.param3;

    const float wCosB   = PCT_UNIT_WIDTH  * std::cos(thetaY);
    const float hSinASB = PCT_UNIT_HEIGHT * std::sin(thetaX) * std::sin(thetaY);
    const float fZCASB  = FOCAL_MODE2 * zoom * std::cos(thetaX) * std::sin(thetaY);
    const float hCosA   = PCT_UNIT_HEIGHT * std::cos(thetaX);
    const float fZSinA  = FOCAL_MODE2 * zoom * std::sin(thetaX);
    const float wSinB   = PCT_UNIT_WIDTH  * std::sin(thetaY);
    const float hSinACB = PCT_UNIT_HEIGHT * std::sin(thetaX) * std::cos(thetaY);
    const float fZCACB  = FOCAL_MODE2 * zoom * std::cos(thetaX) * std::cos(thetaY);

    const float p0=p.p[0], p1=p.p[1], p2=p.p[2], p3=p.p[3], p4=p.p[4], p5=p.p[5];

    for (int y = 0; y < outH_; ++y) {
        float* mx = mapX_.ptr<float>(y);
        float* my = mapY_.ptr<float>(y);
        const float v = (float)y / (float)outH_;
        for (int x = 0; x < outW_; ++x) {
            const float u = (float)x / (float)outW_;
            const float positionX = u * imgW;
            const float positionY = (1.0f - v) * imgH;

            float tx =  (positionX - dcx) * wCosB + (positionY - dcy) * hSinASB + fZCASB;
            float ty =  (positionY - dcy) * hCosA - fZSinA;
            float tz = -(positionX - dcx) * wSinB + (positionY - dcy) * hSinACB + fZCACB;
            tx = -tx; ty = -ty;

            const float alpha = std::atan2(std::sqrt(tx*tx + ty*ty), tz);
            const float beta  = (tx != 0.0f) ? std::atan2(ty, tx)
                                             : ((ty >= 0) ? PI*0.5f : -PI*0.5f);

            const float poly = poly6(alpha, p0,p1,p2,p3,p4,p5);
            const float senH = icx * p.calibrationRatio
                             - poly * p.calibrationRatio * p.calibrationRatio * std::cos(beta);
            const float senV = icy
                             - poly * p.calibrationRatio * p.calibrationRatio * std::sin(beta);
            const float sx = senH / p.calibrationRatio;
            const float sy = senV;
            if (sx >= 0 && sx < imgW && sy >= 0 && sy < imgH) { mx[x] = sx; my[x] = sy; }
            else                                              { mx[x] = -1; my[x] = -1; }
        }
    }
}

void AnypointRemap::fillTube(const AnypointParams& p) {
    const float imgW = (float)fishW_;
    const float imgH = (float)fishH_;
    const float icx  = p.iCx * p.calibrationRatio;
    const float icy  = p.iCy * p.calibrationRatio;
    const float D2R  = PI / 180.0f;
    const float R2D  = 180.0f / PI;
    const float alpha_min = p.param1;
    const float alpha_max = p.param2;

    const float h0 = 90.0f - alpha_min;
    const float h1 = 90.0f - alpha_max;
    const float z0 = std::tan(h0 * D2R);
    const float z1 = std::tan(h1 * D2R);
    const float zLen = z0 - z1;
    const float p0=p.p[0], p1=p.p[1], p2=p.p[2], p3=p.p[3], p4=p.p[4], p5=p.p[5];

    for (int y = 0; y < outH_; ++y) {
        float* mx = mapX_.ptr<float>(y);
        float* my = mapY_.ptr<float>(y);
        const float row = (float)y / (float)outH_;
        for (int x = 0; x < outW_; ++x) {
            const float col = (float)x / (float)outW_;
            float a_alpha = 90.0f - std::atan(z0 - zLen * row) * R2D;
            float a_alpha_rad = a_alpha * D2R;
            float a_beta = col * 2.0f * PI;
            a_beta = PI * 0.5f - a_beta;

            const float poly = poly6(a_alpha_rad, p0,p1,p2,p3,p4,p5);
            const float alpha_ih = poly * p.calibrationRatio;
            const float sx = std::round(icx - alpha_ih * std::cos(a_beta));
            const float sy = std::round(icy - alpha_ih * std::sin(a_beta));
            mx[x] = std::max(0.0f, std::min(imgW - 1.0f, sx));
            my[x] = std::max(0.0f, std::min(imgH - 1.0f, sy));
        }
    }
}

void AnypointRemap::fillCar(const AnypointParams& p) {
    const float imgW = (float)fishW_;
    const float imgH = (float)fishH_;
    const float icx  = p.iCx * p.calibrationRatio;
    const float icy  = p.iCy * p.calibrationRatio;
    const float alpha_max = p.param1;
    const float iC_alpha  = p.param2;
    const float iC_beta   = p.param3;

    const float left_crop=0, right_crop=0.75f, top_crop=0, bottom_crop=1.0f;
    const float iC_alpha_pivot =  iC_alpha * PI / 180.0f;
    const float iC_beta_pivot  = -iC_beta  * PI / 180.0f;

    const float kx = std::sin(iC_alpha_pivot) * std::cos(iC_beta_pivot);
    const float ky = std::sin(iC_alpha_pivot) * std::sin(iC_beta_pivot);
    const float kz = std::cos(iC_alpha_pivot);
    const float p0=p.p[0], p1=p.p[1], p2=p.p[2], p3=p.p[3], p4=p.p[4], p5=p.p[5];

    for (int y = 0; y < outH_; ++y) {
        float* mx = mapX_.ptr<float>(y);
        float* my = mapY_.ptr<float>(y);
        const float v = (float)y / (float)outH_;
        for (int x = 0; x < outW_; ++x) {
            const float u = (float)x / (float)outW_;
            const float row = top_crop  + v * (bottom_crop - top_crop);
            const float col = left_crop + u * (right_crop  - left_crop);

            const float ing_alpha = row * alpha_max * PI / 180.0f;
            const float target_alpha = iC_alpha_pivot + ing_alpha;
            const float Vx = std::sin(target_alpha) * std::cos(iC_beta_pivot);
            const float Vy = std::sin(target_alpha) * std::sin(iC_beta_pivot);
            const float az = std::cos(target_alpha);
            const float kxa_x = ky*az - kz*Vy;
            const float kxa_y = kz*Vx - kx*az;
            const float kxa_z = kx*Vy - ky*Vx;
            const float k_a   = kx*Vx + ky*Vy + kz*az;
            const float ib    = col * 2.0f * PI;
            const float isb   = std::sin(ib);
            const float icb   = std::cos(ib);
            const float Rx = icb*Vx + kxa_x*isb + kx*k_a*(1.0f - icb);
            const float Ry = icb*Vy + kxa_y*isb + ky*k_a*(1.0f - icb);
            const float Rz = icb*az + kxa_z*isb + kz*k_a*(1.0f - icb);
            float fb = std::atan2(Ry, Rx);
            float fa = std::atan2(std::sqrt(Rx*Rx + Ry*Ry), Rz);
            fb = PI * 0.5f - fb;
            const float poly = poly6(fa, p0,p1,p2,p3,p4,p5);
            const float alpha_ih = poly * p.calibrationRatio;
            const float sx = std::round(icx - alpha_ih * std::cos(fb));
            const float sy = std::round(icy - alpha_ih * std::sin(fb));
            if (sx >= 0 && sx < imgW && sy >= 0 && sy < imgH) { mx[x] = sx; my[x] = sy; }
            else                                              { mx[x] = -1; my[x] = -1; }
        }
    }
}

} // namespace dso
