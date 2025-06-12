#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {

static const char *const RUNTIME_TAG = "runtime";

class Component;  // Forward declaration

class ComponentRuntimeStats {
 public:
  ComponentRuntimeStats()
      : period_count_(0),
        total_count_(0),
        period_time_ms_(0),
        total_time_ms_(0),
        period_max_time_ms_(0),
        total_max_time_ms_(0) {}

  void record_time(uint32_t duration_ms) {
    // Update period counters
    this->period_count_++;
    this->period_time_ms_ += duration_ms;
    if (duration_ms > this->period_max_time_ms_)
      this->period_max_time_ms_ = duration_ms;

    // Update total counters
    this->total_count_++;
    this->total_time_ms_ += duration_ms;
    if (duration_ms > this->total_max_time_ms_)
      this->total_max_time_ms_ = duration_ms;
  }

  void reset_period_stats() {
    this->period_count_ = 0;
    this->period_time_ms_ = 0;
    this->period_max_time_ms_ = 0;
  }

  // Period stats (reset each logging interval)
  uint32_t get_period_count() const { return this->period_count_; }
  uint32_t get_period_time_ms() const { return this->period_time_ms_; }
  uint32_t get_period_max_time_ms() const { return this->period_max_time_ms_; }
  float get_period_avg_time_ms() const {
    return this->period_count_ > 0 ? this->period_time_ms_ / static_cast<float>(this->period_count_) : 0.0f;
  }

  // Total stats (persistent until reboot)
  uint32_t get_total_count() const { return this->total_count_; }
  uint32_t get_total_time_ms() const { return this->total_time_ms_; }
  uint32_t get_total_max_time_ms() const { return this->total_max_time_ms_; }
  float get_total_avg_time_ms() const {
    return this->total_count_ > 0 ? this->total_time_ms_ / static_cast<float>(this->total_count_) : 0.0f;
  }

 protected:
  // Period stats (reset each logging interval)
  uint32_t period_count_;
  uint32_t period_time_ms_;
  uint32_t period_max_time_ms_;

  // Total stats (persistent until reboot)
  uint32_t total_count_;
  uint32_t total_time_ms_;
  uint32_t total_max_time_ms_;
};

// For sorting components by run time
struct ComponentStatPair {
  std::string name;
  const ComponentRuntimeStats *stats;

  bool operator>(const ComponentStatPair &other) const {
    // Sort by period time as that's what we're displaying in the logs
    return stats->get_period_time_ms() > other.stats->get_period_time_ms();
  }
};

class RuntimeStatsCollector {
 public:
  RuntimeStatsCollector() : log_interval_(60000), next_log_time_(0), enabled_(true) {}

  void set_log_interval(uint32_t log_interval) { this->log_interval_ = log_interval; }
  uint32_t get_log_interval() const { return this->log_interval_; }

  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  bool is_enabled() const { return this->enabled_; }

  void record_component_time(Component *component, uint32_t duration_ms, uint32_t current_time);

 protected:
  void log_stats_() {
    ESP_LOGI(RUNTIME_TAG, "Component Runtime Statistics");
    ESP_LOGI(RUNTIME_TAG, "Period stats (last %" PRIu32 "ms):", this->log_interval_);

    // First collect stats we want to display
    std::vector<ComponentStatPair> stats_to_display;

    for (const auto &it : this->component_stats_) {
      const ComponentRuntimeStats &stats = it.second;
      if (stats.get_period_count() > 0) {
        ComponentStatPair pair = {it.first, &stats};
        stats_to_display.push_back(pair);
      }
    }

    // Sort by period runtime (descending)
    std::sort(stats_to_display.begin(), stats_to_display.end(), std::greater<ComponentStatPair>());

    // Log top components by period runtime
    for (const auto &it : stats_to_display) {
      const std::string &source = it.name;
      const ComponentRuntimeStats *stats = it.stats;

      ESP_LOGI(RUNTIME_TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms",
               source.c_str(), stats->get_period_count(), stats->get_period_avg_time_ms(),
               stats->get_period_max_time_ms(), stats->get_period_time_ms());
    }

    // Log total stats since boot
    ESP_LOGI(RUNTIME_TAG, "Total stats (since boot):");

    // Re-sort by total runtime for all-time stats
    std::sort(stats_to_display.begin(), stats_to_display.end(),
              [](const ComponentStatPair &a, const ComponentStatPair &b) {
                return a.stats->get_total_time_ms() > b.stats->get_total_time_ms();
              });

    for (const auto &it : stats_to_display) {
      const std::string &source = it.name;
      const ComponentRuntimeStats *stats = it.stats;

      ESP_LOGI(RUNTIME_TAG, "  %s: count=%" PRIu32 ", avg=%.2fms, max=%" PRIu32 "ms, total=%" PRIu32 "ms",
               source.c_str(), stats->get_total_count(), stats->get_total_avg_time_ms(), stats->get_total_max_time_ms(),
               stats->get_total_time_ms());
    }
  }

  void reset_stats_() {
    for (auto &it : this->component_stats_) {
      it.second.reset_period_stats();
    }
  }

  std::map<std::string, ComponentRuntimeStats> component_stats_;
  uint32_t log_interval_;
  uint32_t next_log_time_;
  bool enabled_;
};

// Global instance for runtime stats collection
extern RuntimeStatsCollector runtime_stats;

}  // namespace esphome