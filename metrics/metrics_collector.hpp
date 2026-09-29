#pragma once

#include "frame.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <mutex>
#include <atomic>
#include <cmath>
#include <iostream>
#include <sstream>

namespace cvprofiler {

/**
 * LatencyBuffer — a fixed-capacity rolling window of latency samples.
 * Percentile computation is done on sorted copy — no online approximation.
 * For production you'd replace this with a T-digest for true streaming
 * percentiles, but exact sorting is correct and fast at <100k samples.
 */
class LatencyBuffer {
public:
    explicit LatencyBuffer(size_t capacity = 10000)
        : capacity_(capacity) {
        samples_.reserve(capacity);
    }

    void push(double ms) {
        std::lock_guard<std::mutex> lk(mu_);
        if (samples_.size() >= capacity_) {
/**
 * Evict oldest sample (ring-buffer semantics)
 */
            samples_[write_pos_ % capacity_] = ms;
        } else {
            samples_.push_back(ms);
        }
        ++write_pos_;
        ++total_count_;
        sum_ += ms;
        if (ms > max_) max_ = ms;
        if (min_ < 0 || ms < min_) min_ = ms;
    }

/**
 * Percentile in [0,100]. Returns -1 if no samples.
 */
    double percentile(double p) const {
        std::lock_guard<std::mutex> lk(mu_);
        if (samples_.empty()) return -1.0;
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(std::ceil(p / 100.0 * sorted.size())) - 1;
        idx = std::min(idx, sorted.size() - 1);
        return sorted[idx];
    }

    double mean() const {
        std::lock_guard<std::mutex> lk(mu_);
        if (samples_.empty()) return -1.0;
        return sum_ / static_cast<double>(samples_.size());
    }

    double max() const { std::lock_guard<std::mutex> lk(mu_); return max_; }
    double min() const { std::lock_guard<std::mutex> lk(mu_); return min_; }
    uint64_t count() const { return total_count_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mu_);
        samples_.clear();
        write_pos_ = 0;
        sum_ = 0;
        max_ = 0;
        min_ = -1;
        total_count_ = 0;
    }

private:
    mutable std::mutex mu_;
    std::vector<double> samples_;
    size_t   capacity_;
    size_t   write_pos_ = 0;
    double   sum_ = 0;
    double   max_ = 0;
    double   min_ = -1;
    std::atomic<uint64_t> total_count_{0};
};

/**
 * Per-stage stats snapshot (plain struct — safe to copy/print).
 */
struct StageStats {
    std::string name;
    double mean_process_ms    = 0;
    double p50_process_ms     = 0;
    double p95_process_ms     = 0;
    double p99_process_ms     = 0;
    double max_process_ms     = 0;
    double mean_queue_wait_ms = 0;
    double p95_queue_wait_ms  = 0;
    uint64_t frame_count      = 0;
    uint64_t drop_count       = 0;
    double drop_rate_pct      = 0;
    bool is_bottleneck        = false; // true if this stage has the highest p95
};

/**
 * MetricsCollector — central sink for all pipeline timing data.
 * Usage (called by engine after each frame exits a stage):
 * collector.record(frame, "preprocess");
 * Also supports Mode 2 (probe injection):
 * collector.begin("preprocess");
 * ...your_code...
 * collector.end("preprocess");
 */
class MetricsCollector {
public:
    explicit MetricsCollector(size_t buffer_capacity = 10000)
        : buffer_capacity_(buffer_capacity) {}

/**
 * Mode 1: called by the engine
 */

/**
 * Record timing data from a completed frame for a specific stage.
 */
    void record(const Frame& frame, const std::string& stage_name) {
        auto it = frame.stage_timings.find(stage_name);
        if (it == frame.stage_timings.end()) return;

        const StageTiming& t = it->second;
        auto& entry = get_or_create(stage_name);

        if (t.process_ms >= 0)    entry.process_buf.push(t.process_ms);
        if (t.queue_wait_ms >= 0) entry.queue_buf.push(t.queue_wait_ms);
        if (t.dropped)            ++entry.drop_count;

        ++total_frames_;
    }

/**
 * Record end-to-end latency for a completed frame.
 */
    void record_e2e(double ms) {
        e2e_buf_.push(ms);
    }

/**
 * Mode 2: probe injection API
 * Thread-safe — each thread tracks its own begin time via thread_local.
 */

    void begin(const std::string& stage_name) {
        thread_begin_times_[stage_name] = Clock::now();
    }

    void end(const std::string& stage_name) {
        auto now = Clock::now();
        auto it = thread_begin_times_.find(stage_name);
        if (it == thread_begin_times_.end()) return;
        double ms = Duration(now - it->second).count();
        auto& entry = get_or_create(stage_name);
        entry.process_buf.push(ms);
        ++total_frames_;
    }

/**
 * Snapshot
 */

    std::vector<StageStats> snapshot() const {
        std::lock_guard<std::mutex> lk(registry_mu_);
        std::vector<StageStats> result;
        double worst_p95 = 0;

        for (const auto& [name, entry] : registry_) {
            StageStats s;
            s.name              = name;
            s.mean_process_ms   = entry->process_buf.mean();
            s.p50_process_ms    = entry->process_buf.percentile(50);
            s.p95_process_ms    = entry->process_buf.percentile(95);
            s.p99_process_ms    = entry->process_buf.percentile(99);
            s.max_process_ms    = entry->process_buf.max();
            s.mean_queue_wait_ms = entry->queue_buf.mean();
            s.p95_queue_wait_ms = entry->queue_buf.percentile(95);
            s.frame_count       = entry->process_buf.count();
            s.drop_count        = entry->drop_count.load();
            s.drop_rate_pct     = s.frame_count > 0
                ? 100.0 * s.drop_count / s.frame_count : 0;
            if (s.p95_process_ms > worst_p95) worst_p95 = s.p95_process_ms;
            result.push_back(s);
        }

/**
 * Tag the bottleneck stage (highest p95 processing time)
 */
        for (auto& s : result) {
            if (s.p95_process_ms == worst_p95 && worst_p95 > 0)
                s.is_bottleneck = true;
        }
        return result;
    }

    StageStats e2e_stats() const {
        StageStats s;
        s.name            = "end_to_end";
        s.mean_process_ms = e2e_buf_.mean();
        s.p50_process_ms  = e2e_buf_.percentile(50);
        s.p95_process_ms  = e2e_buf_.percentile(95);
        s.p99_process_ms  = e2e_buf_.percentile(99);
        s.max_process_ms  = e2e_buf_.max();
        s.frame_count     = e2e_buf_.count();
        return s;
    }

    uint64_t total_frames() const { return total_frames_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(registry_mu_);
        for (auto& [name, entry] : registry_) {
            entry->process_buf.reset();
            entry->queue_buf.reset();
            entry->drop_count = 0;
        }
        e2e_buf_.reset();
        total_frames_ = 0;
    }

private:
    struct StageEntry {
        LatencyBuffer process_buf;
        LatencyBuffer queue_buf;
        std::atomic<uint64_t> drop_count{0};
        StageEntry(size_t cap) : process_buf(cap), queue_buf(cap) {}
    };

    StageEntry& get_or_create(const std::string& name) {
        std::lock_guard<std::mutex> lk(registry_mu_);
        auto it = registry_.find(name);
        if (it == registry_.end()) {
            registry_[name] = std::make_unique<StageEntry>(buffer_capacity_);
        }
        return *registry_[name];
    }

    mutable std::mutex registry_mu_;
    std::unordered_map<std::string, std::unique_ptr<StageEntry>> registry_;
    LatencyBuffer e2e_buf_{10000};
    std::atomic<uint64_t> total_frames_{0};
    size_t buffer_capacity_;

/**
 * Mode 2: per-thread begin timestamps (thread_local map via pointer trick)
 */
    static thread_local std::unordered_map<std::string, TimePoint> thread_begin_times_;
};

/**
 * Definition in metrics_collector.cpp
 */
thread_local std::unordered_map<std::string, TimePoint>
    MetricsCollector::thread_begin_times_;

} // namespace cvprofiler
