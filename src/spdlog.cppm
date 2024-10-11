module;

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

export module spdlog;

export namespace spdlog {
using spdlog::critical;
using spdlog::debug;
using spdlog::error;
using spdlog::info;
using spdlog::trace;

using spdlog::set_level;

namespace level {
using spdlog::level::debug;
using spdlog::level::info;
using spdlog::level::trace;
}; // namespace level
}; // namespace spdlog
