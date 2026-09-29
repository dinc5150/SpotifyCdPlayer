#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

// Logs to USB serial and to a RAM ring buffer (served at /api/logs from Phase 2).
// Safe to call from any task, not from ISRs.
namespace logging {

enum class Level : uint8_t { Error, Warn, Info, Debug };

void begin(size_t ringBytes = 16 * 1024);
void setLevel(Level level);
Level level();

void write(Level level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));
void vwrite(Level level, const char *tag, const char *format, va_list args);

// Copies the most recent complete lines, oldest first, into `out` (NUL-terminated).
// Returns the number of characters written.
size_t copyRecent(char *out, size_t capacity);

// Writes to USB serial without dropping bytes while a reader keeps up, and
// without stalling when none does: it waits for buffer space, but gives up
// after `patienceMs` without progress. (Serial's own TX timeout is 0, so a
// plain Serial.write drops whatever doesn't fit in the driver's small buffer.)
void writeSerial(const char *data, size_t length, uint32_t patienceMs = 20);

}  // namespace logging

#define LOG_E(tag, ...) logging::write(logging::Level::Error, tag, __VA_ARGS__)
#define LOG_W(tag, ...) logging::write(logging::Level::Warn, tag, __VA_ARGS__)
#define LOG_I(tag, ...) logging::write(logging::Level::Info, tag, __VA_ARGS__)
#define LOG_D(tag, ...) logging::write(logging::Level::Debug, tag, __VA_ARGS__)
