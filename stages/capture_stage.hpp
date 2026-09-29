#ifndef CAPTURESTAGE_HPP
#define CAPTURESTAGE_HPP

#include "../core/stage.hpp"
#include <opencv2/videoio.hpp>
#include <string>
#include <stdexcept>
#include <iostream>

namespace cvprofiler {

/**
 * @brief: CaptureStage — reads frames from any OpenCV-supported source.
 * Sources:
 * CaptureStage(0) -> webcam index 0
 * CaptureStage("/path/to/video.mp4")
 * CaptureStage("rtsp://...")
 * This is always the first stage in a pipeline.
 * It sets frame.image, frame.capture_time, frame.width, frame.height.
 * When the source is exhausted it sets frame.is_poison = true.
 */
class CaptureStage : public IStage {
public:
    
    // Camera by index
    explicit CaptureStage(int camera_index = 0)
        : source_index_(camera_index), use_index_(true) {}

    // File or line
    explicit CaptureStage(const std::string& source)
        : source_path_(source), use_index_(false) {}

    std::string name() const override { return "capture"; }

    std::string description() const override {
        return use_index_
            ? "Camera index " + std::to_string(source_index_)
            : source_path_;
    }

    void on_start() override {
        if (use_index_) {
            cap_.open(source_index_);
        } else {
            cap_.open(source_path_);
        }
        if (!cap_.isOpened()) {
            throw std::runtime_error("[CaptureStage] Failed to open source: "
                + (use_index_ ? std::to_string(source_index_) : source_path_));
        }
        std::cerr << "[CaptureStage] Opened: " << description() << "\n";
    }

    void process(Frame& frame) override {

        // Timestamp before the blocking read — this is when we decide
        // to capture. the actual decode time is counted as capture latency.
        frame.capture_time = Clock::now();

        bool ok = cap_.read(frame.image);
        if (!ok || frame.image.empty()) {

            // Source exhausted or error — signal pipeline shutdown
            frame = Frame::make_poison();
            return;
        }

        frame.width  = frame.image.cols;
        frame.height = frame.image.rows;
    }

    void on_stop() override {
        if (cap_.isOpened()) cap_.release();
    }

private:
    cv::VideoCapture cap_;
    std::string      source_path_;
    int              source_index_ = 0;
    bool             use_index_    = true;
};

}

#endif