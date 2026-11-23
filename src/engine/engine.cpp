#include "engine/engine.hpp"

namespace slipstream::engine {

Engine::Engine(const EngineConfig& config, IOrderSink& sink) noexcept
    : config_(config), sink_(&sink) {}

}  // namespace slipstream::engine
