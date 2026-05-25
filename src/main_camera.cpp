/**
 * DSO VSLAM – Real-Time Camera (fisheye-aware, low-latency rewrite)
 *
 * Key changes vs previous version:
 *  - Uses dso::Undistort (auto-detects EquiDistant / KannalaBrandt / FOV / RadTan / Pinhole)
 *    so fisheye images are properly rectified before being fed to DSO.
 *  - Camera capture runs in its own thread with a single-slot, drop-old-frame queue:
 *    if the mapping backend can't keep up, we drop frames instead of building latency.
 *  - V4L2 buffer is forced to 1 (CAP_PROP_BUFFERSIZE=1) to avoid the kernel queue
 *    accumulating stale frames.
 *  - uchar -> float conversion uses cv::Mat::convertTo (vectorised) instead of a
 *    scalar pixel loop.
 *  - Default live preset is the FAST one (fewer points / iterations), and
 *    setting_realTimeMaxKF is enabled so KF cadence adapts to backend load.
 *
 * Build:
 *   make -j$(nproc) dso_camera
 *
 * Run examples:
 *   ./dso_camera calib=camera.txt mode=1 cam=0 preset=2 exposure=10
 *   ./dso_camera calib=camera.txt mode=1 cam=0 preset=2 width=640 height=480
 */

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <string>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <opencv2/opencv.hpp>
#include <boost/thread.hpp>

#include "util/settings.h"
#include "util/globalFuncs.h"
#include "util/globalCalib.h"
#include "util/NumType.h"
#include "util/Undistort.h"
#include "util/MinimalImage.h"
#include "util/ImageAndExposure.h"
#include "util/AnypointRemap.h"
#include "util/AnypointParams.h"
#include "FullSystem/FullSystem.h"
#include "OptimizationBackend/MatrixAccumulators.h"
#include "FullSystem/PixelSelector2.h"
#include "IOWrapper/Output3DWrapper.h"
#include "IOWrapper/ImageDisplay.h"
#include "IOWrapper/Pangolin/PangolinDSOViewer.h"
#include "IOWrapper/OutputWrapper/SampleOutputWrapper.h"

std::string calib       = "";
std::string vignette    = "";
std::string gammaCalib  = "";
int   cameraId          = 0;
int   capWidth          = 0;       // 0 = let driver pick
int   capHeight         = 0;
float fixedExposure     = 10.0f;   // ms
bool  useSampleOutput   = false;
int   mode              = 1;       // default: no photometric calib (USB cam)
int   presetChosen      = 2;       // default: FAST preset
std::atomic<bool> shouldStop{false};

using namespace dso;

static void my_exit_handler(int s)
{
    printf("Caught signal %d\n", s);
    shouldStop = true;
}

static void installSignalHandler()
{
    struct sigaction sa;
    sa.sa_handler = my_exit_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void settingsDefault(int preset)
{
    printf("\n=============== PRESET Settings: %d ===============\n", preset);
    if (preset == 0 || preset == 1)
    {
        printf("DEFAULT: 2000 active pts, 5-7 KF, 1-6 LM iter/KF\n");
        setting_desiredImmatureDensity = 1500;
        setting_desiredPointDensity    = 2000;
        setting_minFrames              = 5;
        setting_maxFrames              = 7;
        setting_maxOptIterations       = 6;
        setting_minOptIterations       = 1;
        setting_logStuff               = false;
    }
    else // preset 2 / 3 -> FAST
    {
        printf("FAST: 800 active pts, 4-6 KF, 1-4 LM iter/KF\n");
        setting_desiredImmatureDensity = 600;
        setting_desiredPointDensity    = 800;
        setting_minFrames              = 4;
        setting_maxFrames              = 6;
        setting_maxOptIterations       = 4;
        setting_minOptIterations       = 1;
        setting_logStuff               = false;
    }
    // Real-time tuning: let DSO decide KF cadence based on backend load so the
    // capture loop doesn't pile up frames waiting on the mapper.
    setting_realTimeMaxKF      = true;
    setting_kfGlobalWeight     = 1.3f;   // slightly less greedy KF taking

    // Robustness tuning for live, hand-held, fast-moving camera. Defaults are
    // calibrated for slow benchmark sequences; bump them so brief motion blur
    // or imperfect rectification doesn't immediately mark a frame as lost.
    setting_outlierTH             = 25.0f * 25.0f;   // was 12*12  = 144
    setting_outlierTHSumComponent = 75.0f * 75.0f;   // was 50*50  = 2500
    setting_huberTH               = 15.0f;           // was 9
    setting_coarseCutoffTH        = 40.0f;           // was 20
    setting_minPointsRemaining    = 0.03f;           // was 0.05  (keep more frames alive)
    printf("==================================================\n");
}

static void parseArgument(char* arg)
{
    int   option;
    float foption;
    char  buf[1000];

    if (1 == sscanf(arg, "cam=%d",      &option))  { cameraId      = option;  printf("CAMERA ID: %d\n", cameraId);             return; }
    if (1 == sscanf(arg, "width=%d",    &option))  { capWidth      = option;  printf("CAP WIDTH:  %d\n", capWidth);            return; }
    if (1 == sscanf(arg, "height=%d",   &option))  { capHeight     = option;  printf("CAP HEIGHT: %d\n", capHeight);           return; }
    if (1 == sscanf(arg, "exposure=%f", &foption)) { fixedExposure = foption; printf("EXPOSURE: %.2f ms\n", fixedExposure);    return; }

    if (1 == sscanf(arg, "sampleoutput=%d", &option)) { if (option==1) { useSampleOutput=true; printf("SAMPLE OUTPUT!\n"); } return; }
    if (1 == sscanf(arg, "quiet=%d",        &option)) { if (option==1) { setting_debugout_runquiet=true; printf("QUIET!\n"); } return; }
    if (1 == sscanf(arg, "preset=%d",       &option)) { presetChosen = option; return; }
    if (1 == sscanf(arg, "nolog=%d",        &option)) { if (option==1) { setting_logStuff=false; printf("NO LOG!\n"); } return; }
    if (1 == sscanf(arg, "nogui=%d",        &option)) { if (option==1) { disableAllDisplay=true; printf("NO GUI!\n"); } return; }
    if (1 == sscanf(arg, "nomt=%d",         &option)) { if (option==1) { multiThreading=false; printf("NO MT!\n"); } return; }
    if (1 == sscanf(arg, "rec=%d",          &option)) { if (option==0) { disableReconfigure=true; printf("NO RECONF!\n"); } return; }

    if (1 == sscanf(arg, "calib=%s",    buf)) { calib=buf;      printf("Calib:    %s\n", calib.c_str());     return; }
    if (1 == sscanf(arg, "vignette=%s", buf)) { vignette=buf;   printf("Vignette: %s\n", vignette.c_str());  return; }
    if (1 == sscanf(arg, "gamma=%s",    buf)) { gammaCalib=buf; printf("Gamma:    %s\n", gammaCalib.c_str()); return; }

    if (1 == sscanf(arg, "mode=%d", &option))
    {
        mode = option;
        if (option == 0) { printf("PHOTOMETRIC MODE WITH CALIBRATION!\n"); }
        if (option == 1) {
            printf("PHOTOMETRIC MODE WITHOUT CALIBRATION (affine fixed)!\n");
            setting_photometricCalibration = 0;
            // FIX the affine brightness model (a=1, b=0). Optimising it
            // without a prior (value 0) makes it drift unboundedly on the
            // anypoint pipeline — we have seen b reach ~46 after a minute,
            // which produces NaN residuals and a segfault. The fisheye source
            // has consistent exposure, so identity brightness is correct.
            setting_affineOptModeA = -1;
            setting_affineOptModeB = -1;
        }
        if (option == 2) {
            printf("PHOTOMETRIC MODE WITH PERFECT IMAGES!\n");
            setting_photometricCalibration = 0;
            setting_affineOptModeA = -1;
            setting_affineOptModeB = -1;
            setting_minGradHistAdd = 3;
        }
        return;
    }

    printf("could not parse argument \"%s\"!!!!\n", arg);
}

// Single-slot drop-old-frame queue: producer overwrites, consumer takes the
// freshest. Returning nullptr from pop() means "shutdown".
class LatestFrameQueue {
public:
    void push(cv::Mat&& m, double ts) {
        std::lock_guard<std::mutex> lk(mu_);
        slot_      = std::move(m);
        ts_        = ts;
        has_frame_ = true;
        cv_.notify_one();
    }
    bool pop(cv::Mat& out, double& ts) {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&]{ return has_frame_ || stop_; });
        if (stop_ && !has_frame_) return false;
        out = std::move(slot_);
        ts  = ts_;
        has_frame_ = false;
        return true;
    }
    void shutdown() {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
        cv_.notify_all();
    }
private:
    std::mutex              mu_;
    std::condition_variable cv_;
    cv::Mat                 slot_;
    double                  ts_{0.0};
    bool                    has_frame_{false};
    bool                    stop_{false};
};

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "C");
    installSignalHandler();

    for (int i = 1; i < argc; i++) parseArgument(argv[i]);
    settingsDefault(presetChosen);

    if (calib.empty()) {
        printf("ERROR: must supply calib=<path>\n"
               "  Example: ./dso_camera calib=camera.txt mode=1 cam=0 preset=2\n");
        return 1;
    }

    // ── Anypoint pipeline: DSO sees the rectified perspective view ──────────
    // We no longer hand the raw fisheye to DSO's Undistort. Instead, the
    // capture thread runs the same projection as shaders/anypoint_mode*.frag
    // on the CPU (cv::remap LUT), so DSO's pixel selector, gradient tracker
    // and Coarse Initializer all operate on a flat perspective image.
    //
    // The calibration file is now used only to know the input fisheye size;
    // we read it as a simple "<W> <H>" pair on line 2 the way the DSO format
    // expects, defaulting to 640x480 if absent.
    int inW = 640, inH = 480;
    {
        std::ifstream cf(calib);
        if (cf.good()) {
            std::string firstLine;
            std::getline(cf, firstLine);
            cf >> inW >> inH;
            if (inW <= 0 || inH <= 0) { inW = 640; inH = 480; }
        }
    }
    const int outW = (benchmarkSetting_width  > 0) ? benchmarkSetting_width  : inW;
    const int outH = (benchmarkSetting_height > 0) ? benchmarkSetting_height : inH;

    AnypointRemap anypointRemap;
    anypointRemap.init(outW, outH, inW, inH);

    // Seed the shared params with defaults that match the GUI sliders.
    {
        AnypointParams ap = getAnypointParams();
        ap.mode    = 0;        // anypoint_mode1
        ap.param1  = 0.0f;     // alphaOffset
        ap.param2  = 0.0f;     // betaOffset
        ap.param3  = 1.0f;     // zoom
        ap.iCx     = (float)inW * 0.5f;
        ap.iCy     = (float)inH * 0.5f;
        ap.calibrationRatio = 1.0f;
        ap.p[0] = ap.p[1] = ap.p[2] = ap.p[3] = ap.p[4] = 0.0f;
        ap.p[5] = 200.0f;
        ap.poly5 = 200.0f;
        ap.enabled = true;
        setAnypointParams(ap);
    }

    // Override the polynomial from a real fisheye calibration file if present.
    // File format (single line, 6 floats highest-order first):
    //   p0 p1 p2 p3 p4 p5
    // Use Kalibr `pinhole-equi` or OpenCV `cv::fisheye::calibrate` to get these.
    loadAnypointPolynomial("anypoint_calib.txt");

    // Build a one-shot remap so we can read the virtual focal length used
    // for the DSO calibration matrix below.
    {
        cv::Mat dummyIn(inH, inW, CV_8UC1, cv::Scalar(0)), dummyOut;
        anypointRemap.apply(dummyIn, dummyOut);
    }

    Eigen::Matrix3f K = Eigen::Matrix3f::Identity();
    K(0,0) = anypointRemap.fxVirtual();
    K(1,1) = anypointRemap.fyVirtual();
    K(0,2) = anypointRemap.cxVirtual();
    K(1,2) = anypointRemap.cyVirtual();
    setGlobalCalib(outW, outH, K);
    printf("Anypoint virtual pinhole: fx=%.1f fy=%.1f cx=%.1f cy=%.1f  out=%dx%d  (fisheye in=%dx%d)\n",
           K(0,0), K(1,1), K(0,2), K(1,2), outW, outH, inW, inH);

    // ── Open camera ──────────────────────────────────────────────────────────
    cv::VideoCapture cap(cameraId, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        printf("ERROR: cannot open camera %d\n", cameraId);
        return 1;
    }
    const int reqW = capWidth  > 0 ? capWidth  : inW;
    const int reqH = capHeight > 0 ? capHeight : inH;
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  reqW);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, reqH);
    // Critical: keep V4L2 kernel queue at 1 so DSO sees the freshest frame,
    // not a backlog. Without this the visible "lag" grows unboundedly.
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    const int actualW = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    const int actualH = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    printf("Camera %d opened. Capture=%dx%d  Undistort in=%dx%d out=%dx%d\n",
           cameraId, actualW, actualH, inW, inH, outW, outH);

    // ── FullSystem ───────────────────────────────────────────────────────────
    FullSystem* fullSystem = new FullSystem();
    fullSystem->linearizeOperation = false;  // tracking & mapping run concurrently
    // No photometric calibration: anypoint pipeline outputs a synthesised view
    // that doesn't share the original sensor's gamma/vignette.
    fullSystem->setGammaFunction(nullptr);

    IOWrap::PangolinDSOViewer* viewer = nullptr;
    if (!disableAllDisplay) {
        viewer = new IOWrap::PangolinDSOViewer(wG[0], hG[0], false);
        fullSystem->outputWrapper.push_back(viewer);
    }
    if (useSampleOutput)
        fullSystem->outputWrapper.push_back(new IOWrap::SampleOutputWrapper());

    // ── Capture thread: grab → grayscale → push (drop old) ───────────────────
    LatestFrameQueue queue;
    auto tStart = std::chrono::steady_clock::now();

    std::thread captureThread([&]() {
        cv::Mat raw, gray;
        while (!shouldStop) {
            if (!cap.read(raw) || raw.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if (raw.channels() == 3) cv::cvtColor(raw, gray, cv::COLOR_BGR2GRAY);
            else                     gray = raw;

            // (No horizontal flip — the camera is already in world orientation.
            // The post-remap vertical flip in processThread is the only one
            // we need to undo the Flutter-Y-down convention.)

            // Match the fisheye input size the LUT was built against.
            if (gray.cols != inW || gray.rows != inH)
                cv::resize(gray, gray, cv::Size(inW, inH), 0, 0, cv::INTER_AREA);

            const double ts = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - tStart).count();
            queue.push(gray.clone(), ts);
        }
        queue.shutdown();
    });

    // ── Processing thread: pop newest → anypoint remap → addActiveFrame ──────
    std::thread processThread([&]() {
        int frameId         = 0;
        int lostStreak      = 0;          // consecutive frames with isLost==true
        const int LOST_TOLERANCE = 60;    // ~2 s @ 30 FPS before giving up

        // Track parameter changes that invalidate DSO's existing map. Mode
        // and zoom (param3 in mode 1/2) change the virtual pinhole geometry
        // the system was initialised against, so DSO must be reset cleanly
        // instead of letting it crash on inconsistent residuals.
        AnypointParams lastAp = getAnypointParams();

        cv::Mat fish, rect;
        double  ts;
        while (!shouldStop) {
            if (!queue.pop(fish, ts)) break;

            // Detect drastic anypoint changes and request a reset BEFORE
            // feeding DSO the new geometry.
            {
                AnypointParams ap = getAnypointParams();
                const bool modeChanged = (ap.mode != lastAp.mode);
                const bool zoomChanged =
                    (ap.mode == 0 || ap.mode == 1) &&
                    std::fabs(ap.param3 - lastAp.param3) > 0.05f;
                if (modeChanged || zoomChanged) {
                    printf("Anypoint param change (mode %d->%d, zoom %.2f->%.2f) "
                           "-> requesting DSO reset\n",
                           lastAp.mode, ap.mode, lastAp.param3, ap.param3);
                    setting_fullResetRequested = true;
                }
                lastAp = ap;
            }

            // Apply the anypoint projection on CPU. DSO will see the
            // rectified perspective view, not the round fisheye.
            anypointRemap.apply(fish, rect);
            if (rect.empty()) continue;

            // The Flutter-derived shader math uses a Y-down image-space
            // convention that disagrees with OpenCV's row-major top-down
            // layout, so the cv::remap output is vertically inverted
            // relative to the source. Flip it back here so both the live
            // video panel and the KFDepth overlay show right-side-up.
            cv::flip(rect, rect, 0);

            if (!rect.isContinuous()) rect = rect.clone();

            // Build ImageAndExposure directly — no DSO Undistort needed
            // because the remap already produced a flat perspective image
            // matching the virtual pinhole K we passed to setGlobalCalib.
            ImageAndExposure* img = new ImageAndExposure(rect.cols, rect.rows, ts);
            img->exposure_time = (mode == 0) ? fixedExposure : 0.f;
            const int N = rect.cols * rect.rows;
            const uchar* src = rect.data;
            float*       dst = img->image;
            for (int i = 0; i < N; ++i) dst[i] = (float)src[i];

            fullSystem->addActiveFrame(img, frameId);
            delete img;

            // Track how long the system has been in the "lost" state. We do
            // NOT reset on the first lost frame — DSO often recovers within a
            // few frames using its pose predictor. Only after a long
            // continuous streak do we treat it as a hard failure.
            if (fullSystem->isLost) ++lostStreak;
            else                    lostStreak = 0;

            const bool initJustFailed =
                fullSystem->initFailed && frameId < 250;
            const bool needReset =
                initJustFailed
                || setting_fullResetRequested
                || lostStreak >= LOST_TOLERANCE;

            if (needReset) {
                printf("RESETTING DSO (initFailed=%d lostStreak=%d req=%d frame=%d)\n",
                       fullSystem->initFailed, lostStreak,
                       setting_fullResetRequested, frameId);
                auto wraps = fullSystem->outputWrapper;
                delete fullSystem;
                for (auto* ow : wraps) ow->reset();
                fullSystem = new FullSystem();
                fullSystem->linearizeOperation = false;
                fullSystem->setGammaFunction(nullptr);
                fullSystem->outputWrapper  = wraps;
                setting_fullResetRequested = false;
                frameId    = 0;
                lostStreak = 0;
                continue;
            }
            frameId++;
        }

        fullSystem->blockUntilMappingIsFinished();
        fullSystem->printResult("result.txt");
        printf("Done. Result saved to result.txt\n");
    });

    // Pangolin must run on the main thread.
    if (viewer != nullptr) viewer->run();

    shouldStop = true;
    queue.shutdown();
    if (captureThread.joinable()) captureThread.join();
    if (processThread.joinable()) processThread.join();
    cap.release();

    for (auto* ow : fullSystem->outputWrapper) { ow->join(); delete ow; }

    delete fullSystem;
    printf("EXIT NOW!\n");
    return 0;
}
