#include "log_sheriff/summarizer.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace log_sheriff {
namespace {

std::string to_lower_copy(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (unsigned char ch : input) {
    out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

std::string trim_and_collapse_ws(std::string_view input) {
  std::string out;
  out.reserve(input.size());

  std::size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  std::size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  bool previous_was_space = false;
  for (std::size_t i = start; i < end; ++i) {
    const unsigned char ch = static_cast<unsigned char>(input[i]);
    if (std::isspace(ch) != 0) {
      if (!previous_was_space) {
        out.push_back(' ');
      }
      previous_was_space = true;
    } else {
      out.push_back(static_cast<char>(ch));
      previous_was_space = false;
    }
  }

  return out;
}

std::string normalize_line(std::string_view input) {
  const std::string collapsed = trim_and_collapse_ws(input);
  if (collapsed.empty()) {
    return "<empty>";
  }

  std::string out;
  out.reserve(collapsed.size());

  bool in_number = false;
  for (unsigned char ch : collapsed) {
    if (std::isdigit(ch) != 0) {
      if (!in_number) {
        out += "<num>";
        in_number = true;
      }
      continue;
    }

    in_number = false;
    out.push_back(static_cast<char>(ch));
  }

  return out;
}

bool line_has_level(std::string_view line, LogLevel wanted) {
  const std::string lower = to_lower_copy(line);
  switch (wanted) {
    case LogLevel::Error:
      return lower.find("error") != std::string::npos;
    case LogLevel::Warn:
      return lower.find("warn") != std::string::npos;
    case LogLevel::Info:
      return lower.find("info") != std::string::npos;
    case LogLevel::Debug:
      return lower.find("debug") != std::string::npos;
  }
  return false;
}

std::optional<LogLevel> detect_level(std::string_view line) {
  const std::string lower = to_lower_copy(line);
  if (lower.find("error") != std::string::npos) {
    return LogLevel::Error;
  }
  if (lower.find("warn") != std::string::npos) {
    return LogLevel::Warn;
  }
  if (lower.find("info") != std::string::npos) {
    return LogLevel::Info;
  }
  if (lower.find("debug") != std::string::npos) {
    return LogLevel::Debug;
  }
  return std::nullopt;
}

}  // namespace

std::optional<LogLevel> parse_level(std::string_view raw) {
  const std::string lower = to_lower_copy(raw);
  if (lower == "error") {
    return LogLevel::Error;
  }
  if (lower == "warn") {
    return LogLevel::Warn;
  }
  if (lower == "info") {
    return LogLevel::Info;
  }
  if (lower == "debug") {
    return LogLevel::Debug;
  }
  return std::nullopt;
}

std::string_view level_name(LogLevel level) {
  switch (level) {
    case LogLevel::Error:
      return "error";
    case LogLevel::Warn:
      return "warn";
    case LogLevel::Info:
      return "info";
    case LogLevel::Debug:
      return "debug";
  }
  return "unknown";
}

SummaryResult Summarizer::summarize(const SummarizeOptions& options) const {
  if (options.files.empty()) {
    throw std::invalid_argument("no input files supplied");
  }

  std::unordered_map<std::string, std::uint64_t> frequency;

  SummaryResult result;
  for (const std::string& path : options.files) {
    std::ifstream in(path, std::ios::in);
    if (!in.is_open()) {
      throw std::runtime_error("failed to open file: " + path);
    }

    ++result.files_processed;

    std::string line;
    while (std::getline(in, line)) {
      ++result.total_lines;

      if (options.contains.has_value() && line.find(*options.contains) == std::string::npos) {
        continue;
      }

      if (options.level.has_value() && !line_has_level(line, *options.level)) {
        continue;
      }

      ++result.matched_lines;

      if (const auto detected = detect_level(line); detected.has_value()) {
        ++result.matched_by_level[static_cast<std::size_t>(*detected)];
      }

      ++frequency[normalize_line(line)];
    }
  }

  std::vector<TopLine> entries;
  entries.reserve(frequency.size());
  for (auto& [line, count] : frequency) {
    entries.push_back(TopLine{line, count});
  }

  const auto cmp = [](const TopLine& lhs, const TopLine& rhs) {
    if (lhs.count != rhs.count) {
      return lhs.count > rhs.count;
    }
    return lhs.normalized_line < rhs.normalized_line;
  };

  const std::size_t limit = std::min(options.top_n, entries.size());
  if (limit == 0) {
    result.top_lines.clear();
    return result;
  }

  std::partial_sort(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(limit), entries.end(), cmp);
  entries.resize(limit);
  result.top_lines = std::move(entries);

  return result;
}

}  // namespace log_sheriff
