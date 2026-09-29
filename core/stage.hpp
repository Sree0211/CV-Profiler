#ifndef STAGE_HPP
#define STAGE_HPP

#include "frame.hpp"
#include <string>
#include <memory>

namespace cvprofiler {

/**
 * @brief IStage — the only contract a processing stage must satisfy.
 * How to implement:
 * 1. Subclass IStage.
 * 2. Override name() to return a unique string identifier.
 * 3. Override process(Frame&) with your CV logic.
 * 4. Optionally override on_start() / on_stop() for resource init/cleanup.
 * What NOT to do in process():
 * Do not record timestamps — the engine does this for you.
 * Do not access queues — the engine manages queues.
 * Do not call sleep() unless you are deliberately simulating load.
 * Thread safety:
 * process() is called from a single dedicated thread per stage.
 * on_start() / on_stop() are called from the engine's control thread.
 * If a stage holds shared state, guard it yourself.
 */
class IStage {
public:
    virtual ~IStage() = default;

    // Name. Used as key in metrics and logs.
    // Must be stable across runs for benchmark comparison to be valid.
    virtual std::string name() const = 0;

    // Core processing call. The engine measures clock time around this.
    // Modify frame in-place. Do not replace frame.image with a different Mat
    // without cloning — the original buffer may be referenced downstream.
    virtual void process(Frame& frame) = 0;

    // Called once before the pipeline starts. Use for GPU context init,
    // model loading, file handles, etc.
    virtual void on_start() {}

    // Called after the pipeline stops. Used for cleanup.
    virtual void on_stop() {}

    // Optional: return a human-readable description for dashboard display.
    virtual std::string description() const { return ""; }
};

using StagePtr = std::shared_ptr<IStage>;

}

#endif // ! STAGE_HPP