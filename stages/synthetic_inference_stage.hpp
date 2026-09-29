#ifndef SYNTHETICINFERENCESTAGE_HPP
#define SYNTHETICINFERENCESTAGE_HPP

#include "../core/stage.hpp"
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <cmath>
#include <iostream>

namespace cvprofiler {

/**
 * @brief: SyntheticInferenceStage — simulates a model inference call.
 * Why this exists:
 * In Phase 1 we don't always have a real model. This stage lets you
 * demonstrate the profiler working with controllable latency patterns.
 * It is also the primary tool for stress-testing the pipeline:
 * set base_ms=20 to simulate a fast model (50 FPS throughput)
 * set base_ms=80 to simulate a slow model (12 FPS throughput)
 * set jitter_ms=30 to simulate unstable inference (GPU contention)
 * set spike_probability=0.05 to simulate occasional 10x spikes
 * Profile: the stage writes a fake detection count to frame.metadata.
 */
class SyntheticInferenceStage : public IStage {
public:
    struct Config {
        double base_ms         = 20.0;  // mean inference time
        double jitter_ms       = 5.0;   // std dev of normal jitter
        double spike_ms        = 200.0; // duration of a latency spike
        double spike_prob      = 0.02;  // probability of a spike per frame (0–1)
        int    fake_detections = 3;     // written to metadata for downstream stages
    };

    explicit SyntheticInferenceStage(Config cfg = {})
        : cfg_(cfg)
        , rng_(std::random_device{}())
        , jitter_dist_(0.0, cfg.jitter_ms)
        , spike_dist_(0.0, 1.0)
    {}

    std::string name() const override { return "inference"; }

    std::string description() const override {
        return "synthetic  base=" + fmt(cfg_.base_ms)
             + "ms  jitter=" + fmt(cfg_.jitter_ms)
             + "ms  spike_prob=" + fmt(cfg_.spike_prob * 100) + "%";
    }

    void process(Frame& frame) override {
        
        // Set sleep duration
        double sleep_ms = cfg_.base_ms + jitter_dist_(rng_);
        sleep_ms = std::max(0.0, sleep_ms); // clamp negative jitter

        // Occasional spike (simulates GPU memory pressure, context switch, etc.)
        if (spike_dist_(rng_) < cfg_.spike_prob) {
            sleep_ms += cfg_.spike_ms;
            frame.metadata["inference_spike"] = "true";
        }

        std::this_thread::sleep_for(
            std::chrono::microseconds(static_cast<int64_t>(sleep_ms * 1000)));

        // Fake result metadata
        frame.metadata["detections"] = std::to_string(cfg_.fake_detections);
        frame.metadata["model"]      = "synthetic";
    }

    // Reconfigure at runtime (safe only between frames in single-thread mode)
    void set_base_ms(double ms) { cfg_.base_ms = ms; }
    void set_jitter_ms(double ms) {
        cfg_.jitter_ms = ms;
        jitter_dist_ = std::normal_distribution<double>(0.0, ms);
    }

private:
    static std::string fmt(double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << v;
        return ss.str();
    }

    Config cfg_;
    std::mt19937                         rng_;
    std::normal_distribution<double>     jitter_dist_;
    std::uniform_real_distribution<double> spike_dist_;
};

}

#endif // !SYNTHETICINFERENCESTAGE_HPP
