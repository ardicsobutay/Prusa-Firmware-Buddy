#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <common/delayed_print_manager.hpp>
#include <Marlin/src/gcode/parser.h>

/** \addtogroup G-Codes
 * @{
 */

/**
 * ### M3400: Schedule delayed print start
 *
 * Schedule a print to start after a delay or at a specific time.
 *
 * #### Usage
 *
 *     M3400 D[minutes] F[filepath]           ; Start in D minutes
 *     M3400 T[timestamp] F[filepath]         ; Start at Unix timestamp T
 *     M3400 H[hour] M[minute] F[filepath]    ; Start at specific time today
 *     M3400 C                                ; Cancel scheduled print
 *     M3400                                  ; Query current schedule
 *
 * #### Parameters
 *
 * - `D` - Delay in minutes before starting print
 * - `T` - Unix timestamp when to start print
 * - `H` - Hour (0-23) to start print today
 * - `M` - Minute (0-59) to start print today
 * - `F` - Filepath to G-code file to print (required unless canceling)
 * - `C` - Cancel currently scheduled print
 * - `P` - Preheat minutes before print (optional, default 0)
 *
 * #### Examples
 *
 *     M3400 D30 F"/usb/test.gcode" P5      ; Start in 30 min, preheat 5 min before
 *     M3400 H9 M30 F"/usb/model.gcode"     ; Start at 9:30 AM today
 *     M3400 C                               ; Cancel scheduled print
 *     M3400                                 ; Show current schedule
 */
void PrusaGcodeSuite::M3400() {
    using namespace delayed_print;

    // Cancel scheduled print
    if (parser.seen('C')) {
        DelayedPrintManager::instance().cancel_scheduled_print();
        SERIAL_ECHOLNPGM("Scheduled print canceled");
        return;
    }

    // Query current schedule
    if (!parser.seen('D') && !parser.seen('T') && !parser.seen('H')) {
        if (DelayedPrintManager::instance().has_scheduled_print()) {
            const auto &config = DelayedPrintManager::instance().get_config();
            int32_t remaining = DelayedPrintManager::instance().get_seconds_remaining();

            SERIAL_ECHOPGM("Scheduled print: ");
            SERIAL_ECHO(config.filepath);
            SERIAL_ECHOPGM(" in ");
            SERIAL_ECHO(remaining / 60);
            SERIAL_ECHOLNPGM(" minutes");
        } else {
            SERIAL_ECHOLNPGM("No print scheduled");
        }
        return;
    }

    // Get filepath
    if (!parser.seen('F')) {
        SERIAL_ERROR_MSG("M3400: Filepath (F) parameter required");
        return;
    }

    // Extract filepath from F parameter
    // The parser should have the string after F parameter
    char filepath[delayed_print::MAX_FILEPATH_LENGTH];
    if (!parser.string_arg) {
        SERIAL_ERROR_MSG("M3400: No filepath provided");
        return;
    }

    // Copy filepath, removing quotes if present
    const char *src = parser.string_arg;
    size_t i = 0;
    while (*src && i < delayed_print::MAX_FILEPATH_LENGTH - 1) {
        if (*src != '"' && *src != '\'') {
            filepath[i++] = *src;
        }
        src++;
    }
    filepath[i] = '\0';

    // Get optional preheat parameter
    uint32_t preheat_minutes = parser.seen('P') ? parser.value_ulong() : 0;
    bool enable_preheat = preheat_minutes > 0;

    bool success = false;

    // Schedule with delay in minutes
    if (parser.seen('D')) {
        uint32_t delay_minutes = parser.value_ulong();
        if (delay_minutes == 0) {
            SERIAL_ERROR_MSG("M3400: Delay must be > 0");
            return;
        }
        success = DelayedPrintManager::instance().schedule_delayed_print(
            filepath, delay_minutes, preheat_minutes, enable_preheat);

        if (success) {
            SERIAL_ECHOPGM("Print scheduled in ");
            SERIAL_ECHO(delay_minutes);
            SERIAL_ECHOLNPGM(" minutes");
        }
    }
    // Schedule at specific Unix timestamp
    else if (parser.seen('T')) {
        time_t target_time = parser.value_ulong();
        success = DelayedPrintManager::instance().schedule_timed_print(
            filepath, target_time, preheat_minutes, enable_preheat);

        if (success) {
            SERIAL_ECHOLNPGM("Print scheduled at specified time");
        }
    }
    // Schedule at specific hour:minute today
    else if (parser.seen('H')) {
        uint8_t hour = parser.value_byte();
        uint8_t minute = parser.seen('M') ? parser.value_byte() : 0;

        if (hour > 23 || minute > 59) {
            SERIAL_ERROR_MSG("M3400: Invalid time (H must be 0-23, M must be 0-59)");
            return;
        }

        // Calculate target time as today at specified hour:minute
        time_t now = time(nullptr);
        if (now == -1) {
            SERIAL_ERROR_MSG("M3400: RTC not initialized");
            return;
        }

        struct tm *timeinfo = localtime(&now);
        if (!timeinfo) {
            SERIAL_ERROR_MSG("M3400: Failed to get local time");
            return;
        }

        timeinfo->tm_hour = hour;
        timeinfo->tm_min = minute;
        timeinfo->tm_sec = 0;

        time_t target_time = mktime(timeinfo);

        // If target time is in the past today, schedule for tomorrow
        if (target_time <= now) {
            target_time += 24 * 60 * 60; // Add one day
        }

        success = DelayedPrintManager::instance().schedule_timed_print(
            filepath, target_time, preheat_minutes, enable_preheat);

        if (success) {
            SERIAL_ECHOPGM("Print scheduled for ");
            SERIAL_ECHO(hour);
            SERIAL_ECHOPGM(":");
            if (minute < 10)
                SERIAL_ECHO('0');
            SERIAL_ECHOLN(minute);
        }
    }

    if (!success) {
        SERIAL_ERROR_MSG("M3400: Failed to schedule print");
    }
}

/** @}*/
