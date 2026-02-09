#include <CLI/CLI.hpp>

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

#include "log_sheriff/summarizer.hpp"

namespace {

std::string escape_json_string(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (ch < 0x20) {
          out += "?";
        } else {
          out.push_back(static_cast<char>(ch));
        }
    }
  }
  return out;
}

void print_table(const log_sheriff::SummaryResult& result) {
  std::cout << "Files processed: " << result.files_processed << '\n';
  std::cout << "Total lines:    " << result.total_lines << '\n';
  std::cout << "Matched lines:  " << result.matched_lines << '\n';
  std::cout << "Matched by level:"
            << " error=" << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Error)]
            << " warn=" << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Warn)]
            << " info=" << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Info)]
            << " debug=" << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Debug)]
            << '\n';

  std::cout << "\nTop lines:\n";
  if (result.top_lines.empty()) {
    std::cout << "(no matching lines)\n";
    return;
  }

  std::cout << "Rank  Count  Normalized line\n";
  for (std::size_t i = 0; i < result.top_lines.size(); ++i) {
    const auto& entry = result.top_lines[i];
    std::cout << (i + 1) << "     " << entry.count << "      " << entry.normalized_line << '\n';
  }
}

void print_json(const log_sheriff::SummaryResult& result) {
  std::cout << "{\n";
  std::cout << "  \"files_processed\": " << result.files_processed << ",\n";
  std::cout << "  \"total_lines\": " << result.total_lines << ",\n";
  std::cout << "  \"matched_lines\": " << result.matched_lines << ",\n";
  std::cout << "  \"matched_by_level\": {\n";
  std::cout << "    \"error\": " << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Error)] << ",\n";
  std::cout << "    \"warn\": " << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Warn)] << ",\n";
  std::cout << "    \"info\": " << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Info)] << ",\n";
  std::cout << "    \"debug\": " << result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Debug)] << "\n";
  std::cout << "  },\n";
  std::cout << "  \"top_lines\": [\n";

  for (std::size_t i = 0; i < result.top_lines.size(); ++i) {
    const auto& entry = result.top_lines[i];
    std::cout << "    {\"line\": \"" << escape_json_string(entry.normalized_line) << "\", \"count\": "
              << entry.count << "}";
    if (i + 1 < result.top_lines.size()) {
      std::cout << ',';
    }
    std::cout << '\n';
  }

  std::cout << "  ]\n";
  std::cout << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"log-sheriff: stream log files and summarize matching lines"};
  app.require_subcommand(1);

  log_sheriff::SummarizeOptions summarize_options;
  bool print_json_output = false;
  std::string level_raw;
  std::string contains_raw;

  CLI::App* summarize = app.add_subcommand("summarize", "Summarize one or more log files.");
  summarize->add_option("files", summarize_options.files, "Input log files.")->required()->check(CLI::ExistingFile);

  auto* contains_opt = summarize->add_option("--contains", contains_raw, "Filter lines containing this substring.");
  auto* level_opt = summarize->add_option("--level", level_raw, "Filter by level: error|warn|info|debug.")
                        ->check(CLI::IsMember({"error", "warn", "info", "debug"}, CLI::ignore_case));
  summarize->add_option("--top", summarize_options.top_n, "Show top N normalized lines.")
      ->default_val(10)
      ->check(CLI::PositiveNumber);
  summarize->add_flag("--json", print_json_output, "Print JSON output.");

  CLI11_PARSE(app, argc, argv);

  if (*summarize) {
    if (contains_opt->count() > 0) {
      summarize_options.contains = contains_raw;
    }
    if (level_opt->count() > 0) {
      summarize_options.level = log_sheriff::parse_level(level_raw);
      if (!summarize_options.level.has_value()) {
        throw std::invalid_argument("invalid --level value");
      }
    }

    const log_sheriff::Summarizer analyzer;
    const log_sheriff::SummaryResult result = analyzer.summarize(summarize_options);

    if (print_json_output) {
      print_json(result);
    } else {
      print_table(result);
    }
  }

  return 0;
}
