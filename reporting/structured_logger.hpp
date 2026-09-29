#ifndef STRUCTUREDLOGGER_HPP
#define STRUCTUREDLOGGER_HPP

#include "../core/frame.hpp"
#include "../metrics/metrics_collector.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <ctime>
#include <filesystem>

namespace cvprofiler {

/**
 * @brief: StructuredLogger — writes per-frame JSON lines to a log file.
 * Output format (one JSON object per line, newline-delimited):
 * {
 * "frame_id": 42,
 * "e2e_ms": 23.7,
 * "capture_epoch_us": 1718000000000000,
 * "stages": [
 * {"name":"capture",    "process_ms":1.2,  "queue_wait_ms":0.0, "dropped":false},
 * {"name":"preprocess", "process_ms":4.8,  "queue_wait_ms":0.3, "dropped":false},
 * {"name":"inference",  "process_ms":15.6, "queue_wait_ms":1.2, "dropped":false}
 * ]
 * }
 * This format is directly loadable in Python (example: pandas.read_json(lines=True))
 * or JSON parser.
 */
class StructuredLogger {
public:

    // Opens the log file. If path is empty, generates a timestamped name.
    explicit StructuredLogger(const std::string& path = "")
    {
        std::string filepath = path.empty() ? default_path() : path;

        // Ensure parent directory exists
        auto parent = std::filesystem::path(filepath).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        file_.open(filepath, std::ios::out | std::ios::trunc);
        if (!file_.is_open()) {
            std::cerr << "[StructuredLogger] Failed to open: " << filepath << "\n";
        } else {
            filepath_ = filepath;
            std::cerr << "[StructuredLogger] Writing to: " << filepath << "\n";
        }
    }

    ~StructuredLogger() {
        if (file_.is_open()) file_.close();
    }

    const std::string& filepath() const { return filepath_; }

/**
 * Frame callback signature — attach via engine.on_frame_complete()
 */
    void on_frame(const Frame& frame, const MetricsCollector& /*collector*/) {
        if (!file_.is_open()) return;

        std::string line = serialize(frame);

        std::lock_guard<std::mutex> lk(mu_);
        file_ << line << "\n";

        // Clear every 10 frames to balance durability vs I/O cost
        if (++flush_counter_ % 10 == 0) {
            file_.flush();
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lk(mu_);
        if (file_.is_open()) { file_.flush(); }
    }

private:
    static std::string serialize(const Frame& frame) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3);


        // Capture time as microseconds since epoch
        auto epoch_us = std::chrono::duration_cast<std::chrono::microseconds>(
            frame.capture_time.time_since_epoch()).count();

        ss << "{\"frame_id\":" << frame.frame_id
           << ",\"e2e_ms\":"   << frame.e2e_latency_ms()
           << ",\"capture_epoch_us\":" << epoch_us
           << ",\"dropped\":"  << (frame.was_dropped() ? "true" : "false")
           << ",\"stages\":[";

        bool first = true;
        for (const auto& [name, t] : frame.stage_timings) {
            if (!first) ss << ",";
            ss << "{\"name\":" << json_str(name)
               << ",\"process_ms\":"    << t.process_ms
               << ",\"queue_wait_ms\":" << t.queue_wait_ms
               << ",\"dropped\":"       << (t.dropped ? "true" : "false")
               << "}";
            first = false;
        }
        ss << "]}";
        return ss.str();
    }

    static std::string json_str(const std::string& s) {
        return "\"" + s + "\"";
    }

    static std::string default_path() {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ss;
        ss << "logs/cvprofiler_" << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S") << ".jsonl";
        return ss.str();
    }

    std::ofstream   file_;
    std::string     filepath_;
    std::mutex      mu_;
    uint32_t        flush_counter_ = 0;
};

}

#endif