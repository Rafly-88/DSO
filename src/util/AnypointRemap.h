#pragma once
#include <opencv2/core.hpp>
#include "util/AnypointParams.h"

namespace dso {

// CPU-side port of the fragment shaders in shaders/anypoint_*.gl.frag.
// For each output pixel we precompute the (x,y) sample location in the input
// fisheye image and store it in an OpenCV remap LUT; per-frame work is then
// just a single cv::remap call (SIMD-accelerated).
//
// The LUT is rebuilt only when any field of AnypointParams changes between
// frames, so steady-state CPU cost is ~1 ms for 640x480.
class AnypointRemap {
public:
    // Configure once at startup. Output size = the perspective view that
    // DSO will see. fisheyeW/H = size of incoming raw fisheye frames.
    void init(int outW, int outH, int fisheyeW, int fisheyeH);

    // Apply current params to `src` (CV_8UC1 fisheye). Writes a CV_8UC1
    // rectified image of size (outW, outH) to `dst`. Returns false if the
    // remap LUT is empty (init() not called yet).
    bool apply(const cv::Mat& src, cv::Mat& dst);

    // Camera-intrinsic values matching the rectified output, for feeding
    // setGlobalCalib(). Only valid after init().
    float fxVirtual() const { return fxVirtual_; }
    float fyVirtual() const { return fyVirtual_; }
    float cxVirtual() const { return outW_ * 0.5f; }
    float cyVirtual() const { return outH_ * 0.5f; }
    int   outW()      const { return outW_; }
    int   outH()      const { return outH_; }

private:
    void rebuildMap(const AnypointParams& p);
    // Per-mode pixel-to-sample-location math.
    void fillMode1(const AnypointParams& p);
    void fillMode2(const AnypointParams& p);
    void fillTube (const AnypointParams& p);
    void fillCar  (const AnypointParams& p);

    int outW_      = 0;
    int outH_      = 0;
    int fishW_     = 0;
    int fishH_     = 0;
    float fxVirtual_ = 400.0f;
    float fyVirtual_ = 400.0f;

    cv::Mat        mapX_;   // CV_32FC1
    cv::Mat        mapY_;   // CV_32FC1
    AnypointParams cached_; // last params used to build LUT
    bool           cacheValid_ = false;
};

} // namespace dso
