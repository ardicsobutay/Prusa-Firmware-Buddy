/**
 * @file delayed_print_manager.cpp
 * @brief Implementation of delayed print manager
 */

#include "delayed_print_manager.hpp"
#include "marlin_client.hpp"
#include "print_utils.hpp"
#include "time_tools.hpp"
#include "config_store/store_instance.hpp"
#include "log.h"
#include <cstring>
#include <cstdio>

LOG_COMPONENT_REF(MarlinServer);

namespace delayed_print {

DelayedPrintManager &DelayedPrintManager::instance() {
    static DelayedPrintManager instance;
    return instance;
}

void DelayedPrintManager::init() {
    load_config();
    preheat_started_ = false;

    log_info(MarlinServer, "DelayedPrintManager initialized, has_scheduled=%d",
        config_.is_valid());
}

void DelayedPrintManager::load_config() {
    // Load from config store
    config_.type = config_store().delayed_print_type.get();
    config_.enabled = config_store().delayed_print_enabled.get();
    config_.scheduled_time = config_store().delayed_print_time.get();
    config_.preheat_minutes_before = config_store().delayed_print_preheat_minutes.get();
    config_.preheat_enabled = config_store().delayed_print_preheat_enabled.get();

    // Load filepath
    auto stored_path = config_store().delayed_print_filepath.get();
    std::strncpy(config_.filepath, stored_path.data(), MAX_FILEPATH_LENGTH - 1);
    config_.filepath[MAX_FILEPATH_LENGTH - 1] = '\0';
}

void DelayedPrintManager::save_config() {
    // Save to config store
    config_store().delayed_print_type.set(config_.type);
    config_store().delayed_print_enabled.set(config_.enabled);
    config_store().delayed_print_time.set(config_.scheduled_time);
    config_store().delayed_print_preheat_minutes.set(config_.preheat_minutes_before);
    config_store().delayed_print_preheat_enabled.set(config_.preheat_enabled);

    // Save filepath
    std::array<char, MAX_FILEPATH_LENGTH> path_array = {};
    std::strncpy(path_array.data(), config_.filepath, MAX_FILEPATH_LENGTH - 1);
    config_store().delayed_print_filepath.set(path_array);

    log_info(MarlinServer, "DelayedPrintManager config saved");
}

bool DelayedPrintManager::schedule_delayed_print(const char *filepath, uint32_t delay_minutes,
    uint32_t preheat_minutes, bool enable_preheat) {

    if (!filepath || filepath[0] == '\0') {
        log_error(MarlinServer, "Invalid filepath for delayed print");
        return false;
    }

    time_t current_time = time(nullptr);
    if (current_time == -1) {
        log_error(MarlinServer, "RTC not initialized, cannot schedule print");
        return false;
    }

    config_.type = ScheduleType::delay_minutes;
    config_.enabled = true;
    config_.scheduled_time = current_time + (delay_minutes * 60);
    config_.preheat_minutes_before = preheat_minutes;
    config_.preheat_enabled = enable_preheat;
    std::strncpy(config_.filepath, filepath, MAX_FILEPATH_LENGTH - 1);
    config_.filepath[MAX_FILEPATH_LENGTH - 1] = '\0';

    preheat_started_ = false;
    save_config();

    log_info(MarlinServer, "Scheduled delayed print: %s in %u minutes",
        filepath, delay_minutes);

    return true;
}

bool DelayedPrintManager::schedule_timed_print(const char *filepath, time_t target_time,
    uint32_t preheat_minutes, bool enable_preheat) {

    if (!filepath || filepath[0] == '\0') {
        log_error(MarlinServer, "Invalid filepath for timed print");
        return false;
    }

    time_t current_time = time(nullptr);
    if (current_time == -1) {
        log_error(MarlinServer, "RTC not initialized, cannot schedule print");
        return false;
    }

    if (target_time <= current_time) {
        log_error(MarlinServer, "Target time is in the past");
        return false;
    }

    config_.type = ScheduleType::specific_time;
    config_.enabled = true;
    config_.scheduled_time = target_time;
    config_.preheat_minutes_before = preheat_minutes;
    config_.preheat_enabled = enable_preheat;
    std::strncpy(config_.filepath, filepath, MAX_FILEPATH_LENGTH - 1);
    config_.filepath[MAX_FILEPATH_LENGTH - 1] = '\0';

    preheat_started_ = false;
    save_config();

    log_info(MarlinServer, "Scheduled timed print: %s at %ld",
        filepath, (long)target_time);

    return true;
}

void DelayedPrintManager::cancel_scheduled_print() {
    log_info(MarlinServer, "Canceling scheduled print");
    config_.reset();
    preheat_started_ = false;
    save_config();
}

bool DelayedPrintManager::update() {
    if (!config_.is_valid()) {
        return false;
    }

    time_t current_time = time(nullptr);
    if (current_time == -1) {
        log_error(MarlinServer, "RTC not available");
        return false;
    }

    // Check if it's time to start preheating
    if (config_.preheat_enabled && !preheat_started_ && should_start_preheat()) {
        start_preheat();
        preheat_started_ = true;
    }

    // Check if it's time to start the print
    if (should_start_print()) {
        start_scheduled_print();
        return true;
    }

    return false;
}

int32_t DelayedPrintManager::get_seconds_remaining() const {
    if (!config_.is_valid()) {
        return -1;
    }

    time_t current_time = time(nullptr);
    if (current_time == -1) {
        return -1;
    }

    int32_t remaining = config_.scheduled_time - current_time;
    return remaining > 0 ? remaining : 0;
}

bool DelayedPrintManager::get_time_remaining_string(char *buffer, size_t buffer_size) const {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    int32_t seconds = get_seconds_remaining();
    if (seconds < 0) {
        std::snprintf(buffer, buffer_size, "N/A");
        return false;
    }

    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    if (hours > 0) {
        std::snprintf(buffer, buffer_size, "%uh %um %us", hours, minutes, secs);
    } else if (minutes > 0) {
        std::snprintf(buffer, buffer_size, "%um %us", minutes, secs);
    } else {
        std::snprintf(buffer, buffer_size, "%us", secs);
    }

    return true;
}

bool DelayedPrintManager::get_scheduled_time_string(char *buffer, size_t buffer_size) const {
    if (!buffer || buffer_size == 0 || !config_.is_valid()) {
        return false;
    }

    struct tm *timeinfo = localtime(&config_.scheduled_time);
    if (!timeinfo) {
        return false;
    }

    // Format time based on user's time format preference
    TimeFormat format = time_tools::get_time_format();
    if (format == TimeFormat::TF_12H) {
        std::strftime(buffer, buffer_size, "%I:%M %p", timeinfo);
    } else {
        std::strftime(buffer, buffer_size, "%H:%M", timeinfo);
    }

    return true;
}

bool DelayedPrintManager::should_start_preheat() const {
    if (!config_.is_valid() || !config_.preheat_enabled || preheat_started_) {
        return false;
    }

    time_t current_time = time(nullptr);
    if (current_time == -1) {
        return false;
    }

    time_t preheat_time = config_.scheduled_time - (config_.preheat_minutes_before * 60);
    return current_time >= preheat_time;
}

bool DelayedPrintManager::should_start_print() const {
    if (!config_.is_valid()) {
        return false;
    }

    time_t current_time = time(nullptr);
    if (current_time == -1) {
        return false;
    }

    return current_time >= config_.scheduled_time;
}

void DelayedPrintManager::start_scheduled_print() {
    log_info(MarlinServer, "Starting scheduled print: %s", config_.filepath);

    // Start the print using the standard print_begin function
    print_begin(config_.filepath);

    // Clear the schedule
    cancel_scheduled_print();
}

void DelayedPrintManager::start_preheat() {
    log_info(MarlinServer, "Starting preheat for scheduled print");

    // TODO: Implement preheating logic
    // This would involve:
    // 1. Reading filament type from G-code metadata
    // 2. Setting appropriate temperatures
    // 3. Possibly using M109/M190 commands or direct temp setting

    // For now, we just log that preheating would start
    // The actual implementation would depend on how preheating should work
}

} // namespace delayed_print
