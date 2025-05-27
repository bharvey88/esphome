#include "esphome/core/runtime_stats.h"
#include "esphome/core/component.h"

namespace esphome {

RuntimeStatsCollector runtime_stats;

void RuntimeStatsCollector::record_component_time(Component *component, uint32_t duration_ms, uint32_t current_time) {
  if (!this->enabled_ || component == nullptr)
    return;

  const char *component_source = component->get_component_source();
  this->component_stats_[component_source].record_time(duration_ms);

  // If next_log_time_ is 0, initialize it
  if (this->next_log_time_ == 0) {
    this->next_log_time_ = current_time + this->log_interval_;
    return;
  }

  if (current_time >= this->next_log_time_) {
    this->log_stats_();
    this->reset_stats_();
    this->next_log_time_ = current_time + this->log_interval_;
  }
}

}  // namespace esphome