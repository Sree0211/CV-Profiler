#pragma once

#include "frame.hpp"
#include "stage.hpp"
#include "../metrics/metrics_collector.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <iostream>

namespace cvprofiler {

/**
 * @brief PipelineEngine — Phase 1 (single-threaded, synchronous execution).
 * The engine owns:
 * - the ordered list of stages
 * - the MetricsCollector
 * - the run loop
 * 
 * The engine does NOT own:
 * - Main CV logic (that lives in IStage implementations)
 * - the source of frames (the first stage produces them)
 * 
 * Execution model (single-threaded):
 * For each frame:
 * for each stage:
 * t0 = now()
 * stage.process(frame)
 * t1 = now()
 * frame.stage_timings[stage.name()].process_ms = t1 - t0
 * record_e2e(frame)
 * This means all stages run on the same thread, sequentially.
 * 
 * TODO: Next Phase will promote each stage to its own thread with a queue between.
 *       The IStage interface is identical in both phases — your stage code
 *       does not change when you move to multi-threaded execution.
 */

class PipelineEngine {
public:
    PipelineEngine() = default;

/**
 * Add a stage to the end of the pipeline.
 * Stages run in the order they were added.
 */
    void add_stage(StagePtr stage) {
        stages_.push_back(std::move(stage));
    }

/**
 * Access the metrics collector (for Mode 2 injection or attaching a reporter before run()).
 */
    MetricsCollector& metrics() { return metrics_; }
    const MetricsCollector& metrics() const { return metrics_; }

/**
 * Register a per-frame callback fired after all stages complete.
 * Use this to attach visualizers, loggers, or test assertions.
 * Signature: void callback(const Frame&, const MetricsCollector&)
 */
    using FrameCallback = std::function<void(const Frame&, const MetricsCollector&)>; // TODO: Need to verify
    void on_frame_complete(FrameCallback cb) {
        frame_callbacks_.push_back(std::move(cb));
    }

/**
 * Run the pipeline until the first stage signals completion
 * (returns a poison frame) or stop() is called from another thread.
 * The first stage must call frame.is_poison = false on real frames and
 * produce exactly one Frame::make_poison() to signal end-of-stream.
 */
    void run() {
/**
 * Initialise all stages
 */
        for (auto& stage : stages_) {
            stage->on_start();
        }

        running_ = true;
        uint64_t frame_id = 0;

        while (running_) {
            Frame frame;
            frame.frame_id      = frame_id++;
            frame.pipeline_entry = Clock::now();

            // Stage loop
            bool pipeline_broken = false;
            for (auto& stage : stages_) {
                if (!running_) break;

                // Inject timing around the process
                auto t0 = Clock::now();
                stage->process(frame);
                auto t1 = Clock::now();

                double process_ms = Duration(t1 - t0).count();

                StageTiming timing;
                timing.process_ms    = process_ms;
                timing.queue_wait_ms = 0.0; // no queues in single-threaded mode
                frame.stage_timings[stage->name()] = timing;
                
                // Feed metrics
                metrics_.record(frame, stage->name());
                
                // Poison frame to test
                if (frame.is_poison) {
                    pipeline_broken = true;
                    break;
                }
            }

            if (pipeline_broken) break;

            // Record end-to-end latency
            double e2e_ms = frame.e2e_latency_ms();
            metrics_.record_e2e(e2e_ms);

            // Trigger per frame callbacks
            for (auto& cb : frame_callbacks_) {
                cb(frame, metrics_);
            }
        }

        // Stop all stages
        for (auto& stage : stages_) {
            stage->on_stop();
        }
    }


    // Signal the run loop to stop after the current frame.
    // Safe to call from any thread.
    void stop() { running_ = false; }

    bool is_running() const { return running_.load(); }

private:
    std::vector<StagePtr>     stages_;
    MetricsCollector          metrics_;
    std::vector<FrameCallback> frame_callbacks_;
    std::atomic<bool>         running_{false};
};

}
