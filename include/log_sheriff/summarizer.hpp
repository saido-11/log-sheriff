#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace log_sheriff {

enum class LogLevel {
  Error = 0,
  Warn = 1,
  Info = 2,
  Debug = 3,
};

std::optional<LogLevel> parse_level(std::string_view raw);
std::string_view level_name(LogLevel level);

struct SummarizeOptions {
  std::vector<std::string> files;
  std::optional<std::string> contains;
  std::optional<LogLevel> level;
  std::optional<std::string> since;
  std::optional<std::string> until;
  std::size_t top_n = 10;
};

struct TopLine {
  std::string normalized_line;
  std::uint64_t count = 0;
};

struct SummaryResult {
  std::uint64_t files_processed = 0;
  std::uint64_t total_lines = 0;
  std::uint64_t matched_lines = 0;
  std::array<std::uint64_t, 4> matched_by_level{0, 0, 0, 0};
  std::vector<TopLine> top_lines;
};

class Summarizer {
 public:
  SummaryResult summarize(const SummarizeOptions& options) const;
};

}  // namespace log_sheriff
