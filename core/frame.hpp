#ifndef FRAME_HPP
#define FRAME_HPP

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <opencv2/core.hpp>
#include <atomic>

namespace cvprofiler {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration  = std::chrono::duration<double, std::milli>; // milliseconds

/**
 * Timing record for a single stage on a single frame.
 * Both process_ms and queue_wait_ms default to -1 (unmeasured).
 */
struct StageTiming {
    double process_ms   = -1.0;  // time inside stage::process()
    double queue_wait_ms = -1.0; // time spent waiting in the queue before this stage
    bool   dropped      = false; // frame was dropped at this stage's queue
};

/**
 * @brief Frame — the unit of work flowing through the pipeline.
 * Carry rules:
 * image is a shallow-copy cv::Mat by default (no memcpy on assignment).
 * Call frame.image.clone() explicitly when you need ownership.
 * All timing fields are populated BY the engine, not by stage code.
 * metadata is a free-form key/value bag for stage-specific annotations.
 */
struct Frame {
    uint64_t frame_id  = 0;
    bool     is_poison = false; // sentinel: signals pipeline shutdown

    cv::Mat image;              // shallow ref by default; clone when needed
    int     width  = 0;
    int     height = 0;

    TimePoint capture_time;     // set by the capture stage 
    TimePoint pipeline_entry;   // set by the engine when the frame enters stage 0

    //per-stage diagnostics (populated by engine)
    std::unordered_map<std::string, StageTiming> stage_timings;

    // free-form metadata (set by stages)
    std::unordered_map<std::string, std::string> metadata;

    //End-to-end latency from capture to now (call at pipeline exit).
    double e2e_latency_ms() const {
        auto now = Clock::now();
        return Duration(now - capture_time).count();
    }

    // Total processing time across all stages (excludes queue waits).
    double total_processing_ms() const {
        double sum = 0.0;
        for (const auto& [name, t] : stage_timings) {
            if (t.process_ms >= 0) sum += t.process_ms;
        }
        return sum;
    }

    // Total queue wait time across all stages.
    double total_queue_wait_ms() const {
        double sum = 0.0;
        for (const auto& [name, t] : stage_timings) {
            if (t.queue_wait_ms >= 0) sum += t.queue_wait_ms;
        }
        return sum;
    }

    bool was_dropped() const {
        for (const auto& [name, t] : stage_timings) {
            if (t.dropped) return true;
        }
        return false;
    }

    // Factory: create a test frame to shut down the pipeline.
    static Frame make_poison() {
        Frame f;
        f.is_poison = true;
        return f;
    }
};

}

#endif