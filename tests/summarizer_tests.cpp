#include "log_sheriff/summarizer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <string>
#include <vector>

namespace {

std::string write_temp_log(std::string_view name_prefix, std::string_view content) {
  static std::uint64_t counter = 0;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      (std::string{name_prefix} + "_" + std::to_string(counter++) + ".log");
  std::ofstream out(path);
  out << content;
  out.close();
  return path.string();
}

}  // namespace

TEST_CASE("summarize streams all lines and builds top frequencies", "[summarize]") {
  const std::string path = write_temp_log(
      "log_sheriff_sample_a",
      "INFO request id=100 took 12ms\n"
      "INFO request id=101 took 45ms\n"
      "ERROR failed request id=200\n"
      "DEBUG cache miss key=abc\n");

  log_sheriff::SummarizeOptions options;
  options.files = {path};
  options.top_n = 2;

  const log_sheriff::Summarizer summarizer;
  const log_sheriff::SummaryResult result = summarizer.summarize(options);

  REQUIRE(result.files_processed == 1);
  REQUIRE(result.total_lines == 4);
  REQUIRE(result.matched_lines == 4);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Info)] == 2);
  REQUIRE(result.top_lines.size() == 2);
  REQUIRE(result.top_lines[0].count == 2);
  REQUIRE(result.top_lines[0].normalized_line == "INFO request id=<num> took <num>ms");
}

TEST_CASE("contains and level filters are applied", "[summarize]") {
  const std::string path = write_temp_log(
      "log_sheriff_sample_b",
      "[INFO] connected user=100\n"
      "[WARN] connected user=101\n"
      "[ERROR] disconnected user=101\n"
      "[WARN] connected user=999\n");

  log_sheriff::SummarizeOptions options;
  options.files = {path};
  options.contains = "connected";
  options.level = log_sheriff::LogLevel::Warn;
  options.top_n = 5;

  const log_sheriff::Summarizer summarizer;
  const log_sheriff::SummaryResult result = summarizer.summarize(options);

  REQUIRE(result.total_lines == 4);
  REQUIRE(result.matched_lines == 2);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Warn)] == 2);
  REQUIRE(result.top_lines.size() == 1);
  REQUIRE(result.top_lines[0].normalized_line == "[WARN] connected user=<num>");
  REQUIRE(result.top_lines[0].count == 2);
}

TEST_CASE("summarize supports multiple files", "[summarize]") {
  const std::string path1 = write_temp_log(
      "log_sheriff_sample_c1.log",
      "INFO one\n"
      "ERROR two\n");
  const std::string path2 = write_temp_log(
      "log_sheriff_sample_c2",
      "INFO three\n"
      "INFO four\n");

  log_sheriff::SummarizeOptions options;
  options.files = {path1, path2};
  options.top_n = 10;

  const log_sheriff::Summarizer summarizer;
  const log_sheriff::SummaryResult result = summarizer.summarize(options);

  REQUIRE(result.files_processed == 2);
  REQUIRE(result.total_lines == 4);
  REQUIRE(result.matched_lines == 4);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Info)] == 3);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Error)] == 1);
}

TEST_CASE("parse_level is case-insensitive", "[summarize]") {
  REQUIRE(log_sheriff::parse_level("ERROR") == log_sheriff::LogLevel::Error);
  REQUIRE(log_sheriff::parse_level("Warn") == log_sheriff::LogLevel::Warn);
  REQUIRE(log_sheriff::parse_level("Info") == log_sheriff::LogLevel::Info);
  REQUIRE(log_sheriff::parse_level("debug") == log_sheriff::LogLevel::Debug);
  REQUIRE_FALSE(log_sheriff::parse_level("trace").has_value());
}

TEST_CASE("time range filters are inclusive and exclude lines without timestamps", "[summarize]") {
  const std::string path = write_temp_log(
      "log_sheriff_sample_time_a",
      "2026-02-09T18:01:02Z INFO before range\n"
      "2026-02-09T18:01:03Z INFO at since\n"
      "INFO line without timestamp\n"
      "2026-02-09T18:01:04Z WARN in range\n"
      "2026-02-09T18:01:05Z ERROR at until\n"
      "2026-02-09T18:01:06Z INFO after range\n");

  log_sheriff::SummarizeOptions options;
  options.files = {path};
  options.since = "2026-02-09T18:01:03Z";
  options.until = "2026-02-09T18:01:05Z";
  options.top_n = 10;

  const log_sheriff::Summarizer summarizer;
  const log_sheriff::SummaryResult result = summarizer.summarize(options);

  REQUIRE(result.total_lines == 6);
  REQUIRE(result.matched_lines == 3);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Info)] == 1);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Warn)] == 1);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Error)] == 1);
}

TEST_CASE("lines without timestamps are kept when no time filters are set", "[summarize]") {
  const std::string path = write_temp_log(
      "log_sheriff_sample_time_b",
      "2026-02-09T18:01:02Z INFO with timestamp\n"
      "INFO line without timestamp\n"
      "2026-02-09T18:01:03Z WARN with timestamp\n");

  log_sheriff::SummarizeOptions options;
  options.files = {path};
  options.top_n = 10;

  const log_sheriff::Summarizer summarizer;
  const log_sheriff::SummaryResult result = summarizer.summarize(options);

  REQUIRE(result.total_lines == 3);
  REQUIRE(result.matched_lines == 3);
}

TEST_CASE("time filters support local timestamp format", "[summarize]") {
  const std::string path = write_temp_log(
      "log_sheriff_sample_time_c",
      "2026-02-09 18:01:00 INFO one\n"
      "2026-02-09 18:01:01 INFO two\n"
      "2026-02-09 18:01:02 INFO three\n");

  log_sheriff::SummarizeOptions options;
  options.files = {path};
  options.since = "2026-02-09 18:01:01";
  options.until = "2026-02-09 18:01:02";
  options.top_n = 10;

  const log_sheriff::Summarizer summarizer;
  const log_sheriff::SummaryResult result = summarizer.summarize(options);

  REQUIRE(result.total_lines == 3);
  REQUIRE(result.matched_lines == 2);
  REQUIRE(result.matched_by_level[static_cast<std::size_t>(log_sheriff::LogLevel::Info)] == 2);
}
