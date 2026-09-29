#ifndef PREPROCESSSTAGE_HPP
#define PREPROCESSSTAGE_HPP

#include "../core/stage.hpp"
#include <opencv2/imgproc.hpp>
#include <string>

namespace cvprofiler {

/**
 * @brief: PreprocessStage — standard CV preprocessing pipeline.
 * Operations (in order):
 * 1. Resize to target_w x target_h
 * 2. Color conversion BGR → RGB (for model inference)
 * 3. Pixel normalization to [0, 1] float32
 * This is intentionally done on CPU to demonstrate the preprocessing
 * bottleneck that GPU preprocessing (Later phases) will fix.
 */
class PreprocessStage : public IStage {
public:
    struct Config {
        int    target_w    = 640;
        int    target_h    = 640;
        bool   normalize   = true;   // scale to [0,1] float32
        bool   bgr_to_rgb  = true;   // flip channel order
        int    interp      = cv::INTER_LINEAR;
    };

    explicit PreprocessStage(Config cfg = {}) : cfg_(cfg) {}

    std::string name() const override { return "preprocess"; }

    std::string description() const override {
        return "resize " + std::to_string(cfg_.target_w)
             + "x" + std::to_string(cfg_.target_h)
             + (cfg_.normalize ? " + normalize" : "");
    }

    void process(Frame& frame) override {
        if (frame.image.empty()) return;

        // Resize        
        if (frame.image.cols != cfg_.target_w || frame.image.rows != cfg_.target_h) {
            cv::resize(frame.image, frame.image,
                       cv::Size(cfg_.target_w, cfg_.target_h),
                       0, 0, cfg_.interp);
        }

        // Color conversion
        if (cfg_.bgr_to_rgb) {
            cv::cvtColor(frame.image, frame.image, cv::COLOR_BGR2RGB);
        }

        // Normalize to float32 [0, 1]
        if (cfg_.normalize) {
            frame.image.convertTo(frame.image, CV_32FC3, 1.0 / 255.0);
        }

        frame.metadata["preprocess_size"] =
            std::to_string(cfg_.target_w) + "x" + std::to_string(cfg_.target_h);
    }

private:
    Config cfg_;
};

} 

#endif
