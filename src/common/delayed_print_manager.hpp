/**
 * @file delayed_print_manager.hpp
 * @brief Manager for delayed and scheduled print start functionality
 */

#pragma once

#include <time.h>
#include <stdint.h>
#include <array>

namespace delayed_print {

/// Maximum length for stored file paths
constexpr size_t MAX_FILEPATH_LENGTH = 128;

/// Type of scheduling
enum class ScheduleType : uint8_t {
    none = 0,        ///< No print scheduled
    delay_minutes,   ///< Schedule based on delay in minutes from now
    specific_time,   ///< Schedule for specific time (Unix timestamp)
};

/// Configuration for delayed print (stored in persistent storage)
struct ScheduleConfig {
    ScheduleType type = ScheduleType::none;
    bool enabled = false;
    time_t scheduled_time = 0;               ///< Unix timestamp when print should start
    uint32_t preheat_minutes_before = 0;     ///< Minutes before print to start preheating
    bool preheat_enabled = false;            ///< Whether to preheat before print
    char filepath[MAX_FILEPATH_LENGTH] = {}; ///< Path to the file to print

    /// Reset all fields to default
    void reset() {
        type = ScheduleType::none;
        enabled = false;
        scheduled_time = 0;
        preheat_minutes_before = 0;
        preheat_enabled = false;
        filepath[0] = '\0';
    }

    /// Check if schedule is valid
    bool is_valid() const {
        return enabled && type != ScheduleType::none && filepath[0] != '\0';
    }
};

/**
 * @brief Manager class for delayed print functionality
 *
 * This class manages scheduling of print jobs to start at a later time.
 * It supports both delay-based scheduling (start in X minutes) and
 * time-based scheduling (start at specific time).
 */
class DelayedPrintManager {
public:
    /// Get singleton instance
    static DelayedPrintManager &instance();

    /// Initialize the manager (loads state from persistent storage)
    void init();

    /**
     * @brief Schedule a print to start after a delay
     * @param filepath Path to the G-code file
     * @param delay_minutes Minutes to delay before starting
     * @param preheat_minutes Minutes before print to start preheating
     * @param enable_preheat Whether to enable preheating
     * @return true if successfully scheduled
     */
    bool schedule_delayed_print(const char *filepath, uint32_t delay_minutes,
        uint32_t preheat_minutes = 0, bool enable_preheat = false);

    /**
     * @brief Schedule a print to start at a specific time
     * @param filepath Path to the G-code file
     * @param target_time Unix timestamp when to start
     * @param preheat_minutes Minutes before print to start preheating
     * @param enable_preheat Whether to enable preheating
     * @return true if successfully scheduled
     */
    bool schedule_timed_print(const char *filepath, time_t target_time,
        uint32_t preheat_minutes = 0, bool enable_preheat = false);

    /**
     * @brief Cancel currently scheduled print
     */
    void cancel_scheduled_print();

    /**
     * @brief Check if a print is currently scheduled
     * @return true if a print is scheduled
     */
    bool has_scheduled_print() const {
        return config_.is_valid();
    }

    /**
     * @brief Get the current schedule configuration
     * @return Reference to current config
     */
    const ScheduleConfig &get_config() const {
        return config_;
    }

    /**
     * @brief Update scheduler state - should be called periodically
     *
     * Checks if it's time to start the print or begin preheating.
     * This should be called from the main loop.
     *
     * @return true if a print was started
     */
    bool update();

    /**
     * @brief Get seconds remaining until print starts
     * @return Seconds remaining, or -1 if no print scheduled or time invalid
     */
    int32_t get_seconds_remaining() const;

    /**
     * @brief Get formatted time remaining string
     * @param buffer Buffer to write to
     * @param buffer_size Size of buffer
     * @return true if successful
     */
    bool get_time_remaining_string(char *buffer, size_t buffer_size) const;

    /**
     * @brief Get formatted scheduled start time string
     * @param buffer Buffer to write to
     * @param buffer_size Size of buffer
     * @return true if successful
     */
    bool get_scheduled_time_string(char *buffer, size_t buffer_size) const;

    /**
     * @brief Check if preheating should start now
     * @return true if preheating should start
     */
    bool should_start_preheat() const;

    /**
     * @brief Check if print should start now
     * @return true if print should start
     */
    bool should_start_print() const;

private:
    DelayedPrintManager() = default;
    ~DelayedPrintManager() = default;

    // Prevent copying
    DelayedPrintManager(const DelayedPrintManager &) = delete;
    DelayedPrintManager &operator=(const DelayedPrintManager &) = delete;

    /// Load configuration from persistent storage
    void load_config();

    /// Save configuration to persistent storage
    void save_config();

    /// Start the scheduled print
    void start_scheduled_print();

    /// Start preheating for the scheduled print
    void start_preheat();

    ScheduleConfig config_;
    bool preheat_started_ = false;
};

} // namespace delayed_print
