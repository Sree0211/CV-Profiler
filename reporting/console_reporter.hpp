#ifndef CONSOLEREPORTER_HPP
#define CONSOLEREPORTER_HPP

#include "../metrics/metrics_collector.hpp"
#include "../core/frame.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>

namespace cvprofiler {

/**
 * @brief: ConsoleReporter — pretty-prints per-stage timing to stdout.
 * Two modes:
 * 1. Rolling: prints one line per frame (verbose, good for debugging).
 * 2. Table:   prints a full stats table every N frames (good for overview).
 * Attach as a frame callback:
 * engine.on_frame_complete([&reporter](const Frame& f, const MetricsCollector& m) {
 * reporter.on_frame(f, m);
 * });
 */
class ConsoleReporter {
public:
    explicit ConsoleReporter(uint32_t print_every_n_frames = 30,
                              bool clear_screen = false)
        : print_every_(print_every_n_frames)
        , clear_screen_(clear_screen)
    {}

    void on_frame(const Frame& frame, const MetricsCollector& collector) {
        ++frame_count_;

        // Track FPS
        auto now = Clock::now();
        double elapsed = Duration(now - fps_window_start_).count();
        if (elapsed >= 1000.0) {
            current_fps_ = static_cast<double>(fps_window_frames_) / (elapsed / 1000.0);
            fps_window_start_ = now;
            fps_window_frames_ = 0;
        }
        ++fps_window_frames_;

        if (frame_count_ % print_every_ != 0) return;

        if (clear_screen_) {
            // ANSI clear — works in most terminals
            std::cout << "\033[2J\033[H"; // TODO: Need to verify
        }

        print_table(frame, collector);
    }

    // Call at pipeline end to print final summary.
    void print_summary(const MetricsCollector& collector) const {
        std::cout << "\n";
        print_separator('=');
        std::cout << "  FINAL BENCHMARK SUMMARY\n";
        print_separator('=');
        print_stats_table(collector);
        auto e2e = collector.e2e_stats();
        std::cout << "\n  End-to-end latency:\n";
        std::cout << "    mean=" << fixed2(e2e.mean_process_ms) << "ms"
                  << "  p50="   << fixed2(e2e.p50_process_ms)  << "ms"
                  << "  p95="   << fixed2(e2e.p95_process_ms)  << "ms"
                  << "  p99="   << fixed2(e2e.p99_process_ms)  << "ms"
                  << "  max="   << fixed2(e2e.max_process_ms)  << "ms\n";
        std::cout << "  Total frames processed: " << e2e.frame_count << "\n";
        print_separator('=');
    }

private:
    void print_table(const Frame& frame, const MetricsCollector& collector) const {
        auto stats = collector.snapshot();
        auto e2e   = collector.e2e_stats();

        print_separator('-');
        std::cout << "  CV PIPELINE PROFILER"
                  << "   frame=" << std::setw(6) << frame.frame_id
                  << "   fps=" << std::fixed << std::setprecision(1) << current_fps_
                  << "   e2e=" << fixed2(e2e.mean_process_ms) << "ms (mean)\n";
        print_separator('-');
        
        // Header
        std::cout << std::left
                  << std::setw(18) << "  stage"
                  << std::right
                  << std::setw(10) << "mean(ms)"
                  << std::setw(10) << "p50(ms)"
                  << std::setw(10) << "p95(ms)"
                  << std::setw(10) << "p99(ms)"
                  << std::setw(10) << "q_wait"
                  << std::setw(8)  << "drops"
                  << "  \n";
        print_separator('-');

        for (const auto& s : stats) {
            std::string label = s.is_bottleneck
                ? "  ▶ " + s.name   // bottleneck marker
                : "    " + s.name;
            std::cout << std::left  << std::setw(18) << label
                      << std::right
                      << std::setw(10) << fixed2(s.mean_process_ms)
                      << std::setw(10) << fixed2(s.p50_process_ms)
                      << std::setw(10) << fixed2(s.p95_process_ms)
                      << std::setw(10) << fixed2(s.p99_process_ms)
                      << std::setw(10) << fixed2(s.mean_queue_wait_ms)
                      << std::setw(8)  << s.drop_count;
            if (s.is_bottleneck) std::cout << "  ← BOTTLENECK";
            std::cout << "\n";
        }

        print_separator('-');

        // Mini latency bar graph
        if (!stats.empty()) {
            double total_p95 = 0;
            for (auto& s : stats) total_p95 += std::max(0.0, s.p95_process_ms);
            if (total_p95 > 0) {
                std::cout << "  Latency share (p95):  ";
                for (auto& s : stats) {
                    double share = s.p95_process_ms / total_p95;
                    int bars = static_cast<int>(share * 30);
                    std::string bar(bars, '#');
                    std::cout << s.name.substr(0, 6) << "["
                              << std::setw(30) << std::left << bar << "] "
                              << fixed1(share * 100) << "%  ";
                }
                std::cout << "\n";
            }
        }
        std::cout << std::flush;
    }

    void print_stats_table(const MetricsCollector& collector) const {
        auto stats = collector.snapshot();
        print_separator('-');
        std::cout << std::left  << std::setw(18) << "  stage"
                  << std::right
                  << std::setw(10) << "mean(ms)"
                  << std::setw(10) << "p50(ms)"
                  << std::setw(10) << "p95(ms)"
                  << std::setw(10) << "p99(ms)"
                  << std::setw(10) << "max(ms)"
                  << std::setw(8)  << "frames"
                  << std::setw(8)  << "drops"
                  << "\n";
        print_separator('-');
        for (const auto& s : stats) {
            std::string label = s.is_bottleneck
                ? "  ▶ " + s.name : "    " + s.name;
            std::cout << std::left  << std::setw(18) << label
                      << std::right
                      << std::setw(10) << fixed2(s.mean_process_ms)
                      << std::setw(10) << fixed2(s.p50_process_ms)
                      << std::setw(10) << fixed2(s.p95_process_ms)
                      << std::setw(10) << fixed2(s.p99_process_ms)
                      << std::setw(10) << fixed2(s.max_process_ms)
                      << std::setw(8)  << s.frame_count
                      << std::setw(8)  << s.drop_count;
            if (s.is_bottleneck) std::cout << "  ← BOTTLENECK";
            std::cout << "\n";
        }
        print_separator('-');
    }

    static void print_separator(char c) {
        std::cout << "  " << std::string(90, c) << "\n";
    }

    static std::string fixed2(double v) {
        if (v < 0) return "  -";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    }
    static std::string fixed1(double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << v;
        return ss.str();
    }

    uint32_t     print_every_;
    bool         clear_screen_;
    uint64_t     frame_count_ = 0;
    double       current_fps_ = 0.0;
    TimePoint    fps_window_start_ = Clock::now();
    uint32_t     fps_window_frames_ = 0;
};

}

#endif // !CONSOLEREPORTER_HPP
