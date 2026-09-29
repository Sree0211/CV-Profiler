/**
 * demo_mode1.cpp
 * Mode 1 demonstration: the PipelineEngine owns the execution loop.
 * CV project code lives inside IStage::process() implementations.
 * Run:
 * ./cvprofiler_demo1                    # uses webcam 0
 * ./cvprofiler_demo1 path/to/video.mp4  # uses video file
 * ./cvprofiler_demo1 --synthetic        # no camera needed; runs synthetic load
 * Output:
 * Live table in terminal (updated every 30 frames)
 * JSON lines log in logs/
 */

#include "../core/frame.hpp"
#include "../core/stage.hpp"
#include "../core/pipeline_engine.hpp"
#include "../stages/capture_stage.hpp"
#include "../stages/preprocess_stage.hpp"
#include "../stages/synthetic_inference_stage.hpp"
#include "../stages/postprocess_stage.hpp"
#include "../reporting/console_reporter.hpp"
#include "../reporting/structured_logger.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <opencv2/highgui.hpp>

/**
 * Synthetic capture stage (no camera needed for CI / headless testing)
 */
class SyntheticCaptureStage : public cvprofiler::IStage {
public:
    explicit SyntheticCaptureStage(int w = 1920, int h = 1080,
                                    int max_frames = 500)
        : w_(w), h_(h), max_frames_(max_frames) {}

    std::string name() const override { return "capture"; }

    void process(cvprofiler::Frame& frame) override {
        if (frame_count_ >= max_frames_) {
            frame = cvprofiler::Frame::make_poison();
            return;
        }
/**
 * Simulate camera acquire delay (30 FPS source = ~33ms/frame)
 */
        std::this_thread::sleep_for(std::chrono::milliseconds(33));

        frame.capture_time = cvprofiler::Clock::now();
        frame.image = cv::Mat(h_, w_, CV_8UC3, cv::Scalar(50, 100, 150));

/**
 * Draw a moving rectangle so the image content changes
 */
        int x = static_cast<int>((frame_count_ % w_) * 0.8);
        cv::rectangle(frame.image, cv::Point(x, 100), cv::Point(x + 80, 180),
                      cv::Scalar(0, 255, 0), cv::FILLED);

        frame.width  = w_;
        frame.height = h_;
        ++frame_count_;
    }

private:
    int      w_, h_;
    int      max_frames_;
    uint64_t frame_count_ = 0;
};

/**
 * Global stop flag — set by SIGINT handler so the pipeline can finish
 * the current frame cleanly before shutting down.
 */
static std::atomic<bool> g_stop{false};
static cvprofiler::PipelineEngine* g_engine_ptr = nullptr;

void sigint_handler(int) {
    std::cerr << "\n[demo] Caught SIGINT — stopping pipeline...\n";
    g_stop = true;
    if (g_engine_ptr) g_engine_ptr->stop();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigint_handler);

    bool use_synthetic = false;
    std::string video_source;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--synthetic") {
            use_synthetic = true;
        } else {
            video_source = arg;
        }
    }

    std::cout << "=== CV Pipeline Profiler — Mode 1 Demo ===\n";
    std::cout << "Source: " << (use_synthetic ? "synthetic"
                                : video_source.empty() ? "webcam 0" : video_source)
              << "\n\n";

/**
 * Build the pipeline
 */
    cvprofiler::PipelineEngine engine;
    g_engine_ptr = &engine;

/**
 * 1. Capture
 */
    if (use_synthetic) {
        engine.add_stage(std::make_shared<SyntheticCaptureStage>(1920, 1080, 500));
    } else if (!video_source.empty()) {
        engine.add_stage(std::make_shared<cvprofiler::CaptureStage>(video_source));
    } else {
        engine.add_stage(std::make_shared<cvprofiler::CaptureStage>(0));
    }

/**
 * 2. Preprocess (CPU resize + normalize — intentionally "heavy" for demo)
 */
    cvprofiler::PreprocessStage::Config pp_cfg;
    pp_cfg.target_w  = 640;
    pp_cfg.target_h  = 640;
    pp_cfg.normalize = true;
    engine.add_stage(std::make_shared<cvprofiler::PreprocessStage>(pp_cfg));

/**
 * 3. Synthetic inference — 20ms base, 5ms jitter, occasional 200ms spikes
 */
    cvprofiler::SyntheticInferenceStage::Config inf_cfg;
    inf_cfg.base_ms    = 20.0;
    inf_cfg.jitter_ms  = 5.0;
    inf_cfg.spike_ms   = 200.0;
    inf_cfg.spike_prob = 0.03; // 3% chance per frame
    engine.add_stage(std::make_shared<cvprofiler::SyntheticInferenceStage>(inf_cfg));

/**
 * 4. Postprocess
 */
    engine.add_stage(std::make_shared<cvprofiler::PostprocessStage>());

/**
 * Attach reporters
 */
    cvprofiler::ConsoleReporter reporter(/*print_every=*/30, /*clear_screen=*/false);
    cvprofiler::StructuredLogger logger; // auto timestamped path

    engine.on_frame_complete([&reporter](const cvprofiler::Frame& f,
                                          const cvprofiler::MetricsCollector& m) {
        reporter.on_frame(f, m);
    });

    engine.on_frame_complete([&logger](const cvprofiler::Frame& f,
                                        const cvprofiler::MetricsCollector& m) {
        logger.on_frame(f, m);
    });

/**
 * Run
 */
    engine.run();

/**
 * Final summary
 */
    reporter.print_summary(engine.metrics());
    logger.flush();

    std::cout << "\n[demo] Log written to: " << logger.filepath() << "\n";
    return 0;
}
