# Delayed Print Start Feature

## Overview

The delayed print start feature allows users to schedule print jobs to begin at a later time, either after a specified delay or at a specific time of day. This feature is useful for:

- **Heat Soaking**: Schedule the print to start after the enclosure has had time to warm up
- **Timed Printing**: Start prints at specific times (e.g., in the morning) without being present at the printer
- **Energy Management**: Schedule prints to start during off-peak electricity hours

## Features

- **Delay-based scheduling**: Start print after X minutes
- **Time-based scheduling**: Start print at specific time (HH:MM)
- **Timestamp scheduling**: Start print at Unix timestamp
- **Optional preheating**: Automatically preheat nozzle/bed before print starts
- **Persistent storage**: Scheduled prints survive power cycles (if RTC is configured)
- **Conflict handling**: Automatically cancels scheduled print if user starts another print manually

## Requirements

- **RTC (Real-Time Clock)**: Must be initialized and configured for time-based features to work
- **File access**: The G-code file must be accessible at the scheduled time
- **xBuddy-based printers**: Currently supported on xBuddy-based models (MK4, XL, iX)

## Usage

### G-Code Command: M3400

The M3400 command provides full control over the delayed print scheduler.

#### Schedule print with delay (in minutes)

```gcode
M3400 D30 F"/usb/model.gcode"
```

Starts the print in 30 minutes.

#### Schedule print with delay and preheating

```gcode
M3400 D60 F"/usb/model.gcode" P10
```

Starts the print in 60 minutes, with preheating beginning 10 minutes before the print.

#### Schedule print at specific time (24-hour format)

```gcode
M3400 H9 M30 F"/usb/model.gcode"
```

Starts the print at 9:30 AM. If it's already past 9:30 AM, schedules for 9:30 AM the next day.

#### Schedule print at Unix timestamp

```gcode
M3400 T1709876400 F"/usb/model.gcode"
```

Starts the print at the specified Unix timestamp.

#### Cancel scheduled print

```gcode
M3400 C
```

Cancels any currently scheduled print.

#### Query current schedule

```gcode
M3400
```

Displays information about the currently scheduled print (if any).

### Parameters

| Parameter | Description | Required | Example |
|-----------|-------------|----------|---------|
| `D` | Delay in minutes | Yes (unless using T, H, or C) | `D30` |
| `T` | Unix timestamp | Yes (unless using D, H, or C) | `T1709876400` |
| `H` | Hour (0-23) | Yes with `M` (unless using D, T, or C) | `H9` |
| `M` | Minute (0-59) | Optional with `H` | `M30` |
| `F` | Filepath to G-code | Yes (unless using C) | `F"/usb/test.gcode"` |
| `P` | Preheat minutes before print | Optional | `P10` |
| `C` | Cancel scheduled print | No | `C` |

## Implementation Details

### Architecture

The delayed print feature consists of:

1. **DelayedPrintManager** (`delayed_print_manager.hpp/cpp`): Core scheduling logic
2. **Config Store Integration**: Persistent storage for scheduled print data
3. **Marlin Server Integration**: Background scheduler checking in main loop
4. **M3400 G-code**: User interface for scheduling

### Persistent Storage

Scheduled print information is stored in the config store (EEPROM) and includes:

- Schedule type (delay, specific time, or none)
- Target time (Unix timestamp)
- File path
- Preheat settings
- Enabled flag

This data persists across power cycles as long as the RTC maintains the correct time.

### Scheduler Operation

The scheduler runs in the marlin_server main loop and checks every cycle whether:

1. It's time to start preheating (if enabled)
2. It's time to start the print

When the scheduled time arrives, the scheduler automatically calls `print_begin()` to start the print job.

### Conflict Handling

If a user attempts to start a print manually while another print is scheduled, the scheduled print is automatically canceled. This prevents confusion and ensures the user's immediate action takes precedence.

## Limitations

1. **Single schedule**: Only one print can be scheduled at a time
2. **RTC dependency**: Time-based scheduling requires a properly configured RTC
3. **File availability**: The scheduled file must be accessible when the print starts
4. **No UI integration**: Currently accessible only via G-code (UI integration planned for future release)
5. **Filament detection**: Standard filament detection warnings apply at print start time

## Examples

### Heat Soak Scenario

Preheat the chamber by setting bed/nozzle temperatures manually, then schedule the print:

```gcode
M140 S60        ; Set bed to 60°C
M104 S200       ; Set nozzle to 200°C
M3400 D15 F"/usb/abs_part.gcode"  ; Start in 15 minutes
```

### Morning Print Scenario

Schedule a print to start at 7:00 AM:

```gcode
M3400 H7 M0 F"/usb/morning_print.gcode" P5
```

The printer will preheat at 6:55 AM and start printing at 7:00 AM.

### Query and Cancel Scenario

```gcode
M3400              ; Check what's scheduled
; Output: "Scheduled print: /usb/test.gcode in 25 minutes"

M3400 C            ; Cancel it
; Output: "Scheduled print canceled"

M3400              ; Verify cancellation
; Output: "No print scheduled"
```

## Troubleshooting

### "RTC not initialized" error

**Cause**: The Real-Time Clock is not configured or has lost power.

**Solution**: Configure the RTC through the printer settings menu or via network time synchronization.

### Scheduled print doesn't start

**Possible causes**:
- RTC time is incorrect
- File was moved or removed
- Printer is in an error state
- Filament not loaded

**Solution**: Verify the RTC time is correct, ensure the file exists, and check printer status.

### Print starts without preheating

**Cause**: The `P` parameter was not specified or set to 0.

**Solution**: Add the `P` parameter with desired preheat time: `M3400 D30 F"/usb/test.gcode" P5`

## Safety Considerations

- **Fire safety**: Do not leave the printer unattended for extended periods
- **Filament**: Ensure correct filament is loaded before scheduling
- **Enclosure**: Verify enclosure is properly closed if scheduling heat soak prints
- **Power**: Ensure stable power supply for scheduled prints

## Future Enhancements

Planned improvements for this feature:

- [ ] UI integration with touchscreen
- [ ] Multiple scheduled prints queue
- [ ] Email/notification when print starts
- [ ] Integration with Prusa Connect for remote scheduling
- [ ] Recurring print schedules (daily/weekly)
- [ ] Conditional scheduling (start when temperature/humidity conditions met)

## Technical Reference

### File Locations

- **Manager**: `src/common/delayed_print_manager.hpp` / `.cpp`
- **G-code**: `src/marlin_stubs/M3400.cpp`
- **Config Store**: `src/persistent_stores/store_instances/config_store/store_definition.hpp`
- **Integration**: `src/common/marlin_server.cpp`

### API Reference

See `delayed_print_manager.hpp` for the full `DelayedPrintManager` API.

Key methods:
- `schedule_delayed_print()`: Schedule with minute delay
- `schedule_timed_print()`: Schedule at specific timestamp
- `cancel_scheduled_print()`: Cancel scheduled print
- `has_scheduled_print()`: Check if print is scheduled
- `get_seconds_remaining()`: Get countdown to print start
- `update()`: Background scheduler update (called from main loop)

## Contributing

Contributions to improve this feature are welcome! Areas for contribution:

- UI implementation for easier scheduling
- Additional scheduling modes
- Integration with external services
- Improved error handling and user feedback

## License

This feature is part of the Prusa-Firmware-Buddy project and follows the project's license terms.

## Credits

Feature developed to address issue #4866: Delayed print start via printer UI.

---

For questions or issues, please file a bug report in the Prusa-Firmware-Buddy repository.
