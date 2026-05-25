// Minimal live entry point. Mirrors the fixes in main_camera.cpp:
//  - actually USE the Undistort object (previous version built it but never
//    rectified the frame, which is fatal for fisheye lenses)
//  - V4L2 buffer = 1 so the kernel queue can't accumulate stale frames
//  - capture resolution matches the calib's expected input size
//  - vectorised uchar -> float (handled by Undistort::undistort itself)

#include <opencv2/opencv.hpp>
#include <iostream>

#include "FullSystem/FullSystem.h"
#include "util/Undistort.h"
#include "util/MinimalImage.h"
#include "util/ImageAndExposure.h"
#include "util/globalCalib.h"
#include "util/settings.h"
#include "IOWrapper/Pangolin/PangolinDSOViewer.h"

using namespace dso;

int main(int /*argc*/, char** /*argv*/) {
    // 1. Build undistorter from camera.txt (handles Pinhole / FOV / RadTan /
    //    EquiDistant / KannalaBrandt automatically).
    const std::string calibFile = "camera.txt";
    Undistort* undistorter = Undistort::getUndistorterForFile(calibFile, "", "");
    if (!undistorter || !undistorter->isValid()) {
        std::cerr << "ERROR: cannot load " << calibFile << std::endl;
        return -1;
    }
    const int inW  = undistorter->getOriginalSize()[0];
    const int inH  = undistorter->getOriginalSize()[1];
    const int outW = undistorter->getSize()[0];
    const int outH = undistorter->getSize()[1];
    setGlobalCalib(outW, outH, undistorter->getK().cast<float>());

    // 2. Camera. Request the input resolution implied by the calibration so we
    //    don't resize twice. Force buffer=1 to kill V4L2 latency build-up.
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    if (!cap.isOpened()) { std::cerr << "ERROR: camera not opened\n"; return -1; }
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  inW);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, inH);
    cap.set(cv::CAP_PROP_BUFFERSIZE,   1);

    // 3. Real-time DSO settings: fewer points, adaptive KF cadence.
    setting_desiredImmatureDensity = 600;
    setting_desiredPointDensity    = 800;
    setting_minFrames              = 4;
    setting_maxFrames              = 6;
    setting_maxOptIterations       = 4;
    setting_minOptIterations       = 1;
    setting_realTimeMaxKF          = true;
    setting_photometricCalibration = 0;   // no gamma/vignette supplied
    setting_affineOptModeA         = 0;
    setting_affineOptModeB         = 0;
    setting_logStuff               = false;

    FullSystem* fullSystem = new FullSystem();
    fullSystem->linearizeOperation = false;
    fullSystem->setGammaFunction(
        undistorter->photometricUndist ? undistorter->photometricUndist->getG() : nullptr);

    auto* viewer = new IOWrap::PangolinDSOViewer(outW, outH, false);
    fullSystem->outputWrapper.push_back(viewer);

    std::cout << "==========================================\n"
              << "DSO LIVE READY (fisheye-aware)\n"
              << "Capture " << inW << "x" << inH
              << " -> DSO " << outW << "x" << outH << "\n"
              << "==========================================\n";

    std::thread worker([&]() {
        cv::Mat frame, gray;
        int frameID = 0;
        while (true) {
            if (!cap.read(frame) || frame.empty()) continue;
            if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            else                       gray = frame;
            if (gray.cols != inW || gray.rows != inH)
                cv::resize(gray, gray, cv::Size(inW, inH), 0, 0, cv::INTER_AREA);
            if (!gray.isContinuous()) gray = gray.clone();

            MinimalImageB minImg(gray.cols, gray.rows, gray.data);
            ImageAndExposure* img =
                undistorter->undistort<unsigned char>(&minImg, 0.f, (double)frameID);

            fullSystem->addActiveFrame(img, frameID);
            delete img;
            frameID++;
        }
    });

    // Pangolin owns the main thread.
    viewer->run();

    worker.detach();   // process exits anyway when viewer closes
    delete fullSystem;
    delete undistorter;
    return 0;
}
