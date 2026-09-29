#ifndef POSTPROCESSSTAGE_HPP
#define POSTPROCESSSTAGE_HPP

#include "../core/stage.hpp"
#include <opencv2/imgproc.hpp>
#include <string>

namespace cvprofiler {

/**
 * @brief: PostprocessStage — decodes detection results and draws overlays.
 * In the synthetic demo: reads "detections" from frame.metadata and
 * draws fake bounding boxes. 
 * TODO: In a real pipeline, replace process()
 * internals with the actual NMS / decode logic.
 */
class PostprocessStage : public IStage {
public:
    std::string name() const override { return "postprocess"; }

    void process(Frame& frame) override {
        if (frame.image.empty()) return;

        // Convert back to uint8 if normalized (for display)
        cv::Mat display;
        if (frame.image.type() == CV_32FC3) {
            frame.image.convertTo(display, CV_8UC3, 255.0);
        } else {
            display = frame.image;
        }

        // Convert RGB back to BGR for OpenCV display
        cv::Mat bgr;
        if (display.channels() == 3) {
            cv::cvtColor(display, bgr, cv::COLOR_RGB2BGR);
        } else {
            bgr = display;
        }

        // Simulate NMS decode: draw fake boxes from metadata
        int det_count = 0;
        auto it = frame.metadata.find("detections");
        if (it != frame.metadata.end()) {
            try { det_count = std::stoi(it->second); } catch (...) {}
        }

        // Draw fake detection boxes (for demo purposes)
        int h = bgr.rows, w = bgr.cols;
        for (int i = 0; i < det_count; ++i) {

            // Deterministic fake box positions based on frame_id and index
            int x1 = static_cast<int>((0.1 + 0.25 * i) * w);
            int y1 = static_cast<int>((0.1 + 0.15 * i) * h);
            int x2 = std::min(w - 1, x1 + static_cast<int>(0.2 * w));
            int y2 = std::min(h - 1, y1 + static_cast<int>(0.2 * h));

            cv::rectangle(bgr, cv::Point(x1, y1), cv::Point(x2, y2),
                          cv::Scalar(0, 255, 0), 2);
            cv::putText(bgr, "obj" + std::to_string(i),
                        cv::Point(x1, y1 - 5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(0, 255, 0), 1);
        }

        frame.image   = bgr;
        frame.metadata["postprocess_detections"] = std::to_string(det_count);
    }
};

}

#endif // !POSTPROCESSSTAGE_HPP
