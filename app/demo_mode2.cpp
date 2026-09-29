/**
 * @brief: demo_mode2.cpp
 *  Mode 2 demonstration: existing application loop runs as-is.
 *  You inject profiler.begin() / profiler.end() probes around the
 *  section of code the needs to be measured.
 *  This is the pattern to use when its not needed to
 *  restructure the codebase into IStage implementations.
 *  The profiler collects the same metrics,writes the same logs, 
 *  and shows the same dashboard.
 * 
 */

#include "../core/frame.hpp"
#include "../metrics/metrics_collector.hpp"
#include "../reporting/console_reporter.hpp"
#include "../reporting/structured_logger.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>
#include <atomic>
#include <string>

static std::atomic<bool> g_stop{false};
static void sigint_handler(int) { g_stop = true; }

cv::Mat sample_existing_preprocess(const cv::Mat& input) {
    cv::Mat out;
    cv::resize(input, out, cv::Size(640, 640), 0, 0, cv::INTER_LINEAR);
    cv::cvtColor(out, out, cv::COLOR_BGR2RGB);
    out.convertTo(out, CV_32FC3, 1.0 / 255.0);
    return out;
}

cv::Mat sample_existing_inference(const cv::Mat& input,
                                  std::mt19937& rng,
                                  std::normal_distribution<double>& jitter) {
    
    double sleep_ms = 20.0 + jitter(rng);
    sleep_ms = std::max(1.0, sleep_ms);
    std::this_thread::sleep_for(
        std::chrono::microseconds(static_cast<int64_t>(sleep_ms * 1000)));
    return input;
}

cv::Mat sample_existing_postprocess(const cv::Mat& input) {
    cv::Mat out;
    if (input.type() == CV_32FC3) {
        input.convertTo(out, CV_8UC3, 255.0);
        cv::cvtColor(out, out, cv::COLOR_RGB2BGR);
    } else {
        out = input.clone();
    }
    cv::rectangle(out, cv::Point(50, 50), cv::Point(200, 200),
                  cv::Scalar(0, 255, 0), 2);
    return out;
}

/**
 * The only changes to the existing main():
 * 1. Include the profiler headers (3 lines at top)
 * 2. Call profiler.begin("name") before each section
 * 3. Call profiler.end("name") after each section
 * 4. Call reporter.on_frame() at the end of each loop iteration
 */

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigint_handler);

    std::string video_source = (argc > 1) ? argv[1] : "";

    std::cout << "=== CV Pipeline Profiler — Mode 2 Demo (probe injection) ===\n";
    std::cout << "Source: " << (video_source.empty() ? "synthetic" : video_source) << "\n\n";

    // Current existing setup
    cv::VideoCapture cap;
    bool use_synthetic = video_source.empty();
    if (!use_synthetic) {
        cap.open(video_source);
        if (!cap.isOpened()) {
            std::cerr << "Failed to open: " << video_source << "\n";
            return 1;
        }
    }

    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<double> jitter(0.0, 5.0);

    // profiler setup
    cvprofiler::MetricsCollector profiler;
    cvprofiler::ConsoleReporter  reporter(30, false);
    cvprofiler::StructuredLogger logger;
    uint64_t frame_id = 0;

/**
 * sample existing main loop (mostly unchanged)
 */
    while (!g_stop) {
        // Capture
        profiler.begin("capture");
        cvprofiler::Frame frame;
        frame.capture_time = cvprofiler::Clock::now();
        frame.frame_id     = frame_id++;

        if (use_synthetic) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            frame.image = cv::Mat(1080, 1920, CV_8UC3, cv::Scalar(50, 100, 150));
        } else {
            bool ok = cap.read(frame.image);
            if (!ok || frame.image.empty()) break;
        }
        profiler.end("capture");
        
        // Preprocess
        profiler.begin("preprocess");
        cv::Mat preprocessed = sample_existing_preprocess(frame.image);
        profiler.end("preprocess");

        // Inference
        profiler.begin("inference");
        cv::Mat result = sample_existing_inference(preprocessed, rng, jitter);
        profiler.end("inference");

        // Postprocess
        profiler.begin("postprocess");
        cv::Mat output = sample_existing_postprocess(result);
        profiler.end("postprocess");


        frame.image = output;
        reporter.on_frame(frame, profiler);
        logger.on_frame(frame, profiler);

        if (frame_id >= 500) break; // Sample limit
    }

    // Summary
    reporter.print_summary(profiler);
    logger.flush();

    std::cout << "\n[demo] Log written to: " << logger.filepath() << "\n";
    std::cout << "[demo] Diff: in Mode 2 you added 8 lines to an existing loop.\n";
    return 0;
}
