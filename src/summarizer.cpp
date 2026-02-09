#include "log_sheriff/summarizer.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace log_sheriff {
namespace {

struct ParsedTimestamp {
  std::time_t epoch_seconds = 0;
  std::size_t consumed_chars = 0;
};

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

bool parse_fixed_int(std::string_view input, std::size_t pos, std::size_t len, int& value) {
  if (pos + len > input.size()) {
    return false;
  }

  int out = 0;
  for (std::size_t i = 0; i < len; ++i) {
    const unsigned char ch = static_cast<unsigned char>(input[pos + i]);
    if (std::isdigit(ch) == 0) {
      return false;
    }
    out = out * 10 + (ch - static_cast<unsigned char>('0'));
  }

  value = out;
  return true;
}

bool is_leap_year(int year) {
  if (year % 400 == 0) {
    return true;
  }
  if (year % 100 == 0) {
    return false;
  }
  return year % 4 == 0;
}

int days_in_month(int year, int month) {
  static constexpr int kDaysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 0;
  }
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return kDaysByMonth[month - 1];
}

std::time_t to_time_utc(std::tm tm) {
#if defined(_WIN32)
  return _mkgmtime(&tm);
#else
  return timegm(&tm);
#endif
}

std::string_view trim(std::string_view input) {
  std::size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  std::size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  return input.substr(start, end - start);
}

std::optional<ParsedTimestamp> parse_timestamp_prefix(std::string_view input) {
  if (input.size() < 19) {
    return std::nullopt;
  }

  if (input[4] != '-' || input[7] != '-' || input[13] != ':' || input[16] != ':') {
    return std::nullopt;
  }

  const char separator = input[10];
  if (separator != 'T' && separator != ' ') {
    return std::nullopt;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  if (!parse_fixed_int(input, 0, 4, year) || !parse_fixed_int(input, 5, 2, month) ||
      !parse_fixed_int(input, 8, 2, day) || !parse_fixed_int(input, 11, 2, hour) ||
      !parse_fixed_int(input, 14, 2, minute) || !parse_fixed_int(input, 17, 2, second)) {
    return std::nullopt;
  }

  if (month < 1 || month > 12) {
    return std::nullopt;
  }
  if (day < 1 || day > days_in_month(year, month)) {
    return std::nullopt;
  }
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
    return std::nullopt;
  }

  const bool is_utc_z = separator == 'T';
  std::size_t consumed_chars = 19;
  if (is_utc_z) {
    if (input.size() < 20 || input[19] != 'Z') {
      return std::nullopt;
    }
    consumed_chars = 20;
  }

  if (input.size() > consumed_chars &&
      std::isspace(static_cast<unsigned char>(input[consumed_chars])) == 0) {
    return std::nullopt;
  }

  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = -1;

  std::time_t epoch_seconds = 0;
  if (is_utc_z) {
    epoch_seconds = to_time_utc(tm);
  } else {
    epoch_seconds = std::mktime(&tm);
  }

  if (epoch_seconds == static_cast<std::time_t>(-1)) {
    return std::nullopt;
  }

  return ParsedTimestamp{epoch_seconds, consumed_chars};
}

std::optional<std::time_t> parse_timestamp_exact(std::string_view input) {
  const std::string_view trimmed = trim(input);
  const auto parsed = parse_timestamp_prefix(trimmed);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  if (parsed->consumed_chars != trimmed.size()) {
    return std::nullopt;
  }
  return parsed->epoch_seconds;
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

  const bool has_time_filter = options.since.has_value() || options.until.has_value();
  std::optional<std::time_t> since_bound;
  std::optional<std::time_t> until_bound;
  if (options.since.has_value()) {
    since_bound = parse_timestamp_exact(*options.since);
    if (!since_bound.has_value()) {
      throw std::invalid_argument(
          "invalid --since timestamp; expected YYYY-MM-DDTHH:MM:SSZ or YYYY-MM-DD HH:MM:SS");
    }
  }
  if (options.until.has_value()) {
    until_bound = parse_timestamp_exact(*options.until);
    if (!until_bound.has_value()) {
      throw std::invalid_argument(
          "invalid --until timestamp; expected YYYY-MM-DDTHH:MM:SSZ or YYYY-MM-DD HH:MM:SS");
    }
  }
  if (since_bound.has_value() && until_bound.has_value() && *since_bound > *until_bound) {
    throw std::invalid_argument("--since must be less than or equal to --until");
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

      if (has_time_filter) {
        const auto parsed = parse_timestamp_prefix(line);
        if (!parsed.has_value()) {
          continue;
        }
        if (since_bound.has_value() && parsed->epoch_seconds < *since_bound) {
          continue;
        }
        if (until_bound.has_value() && parsed->epoch_seconds > *until_bound) {
          continue;
        }
      }

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
