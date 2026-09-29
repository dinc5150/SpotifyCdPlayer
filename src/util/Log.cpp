#include "util/Log.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace logging {

namespace {

constexpr char kLetters[] = {'E', 'W', 'I', 'D'};

SemaphoreHandle_t mutex = nullptr;
char *ring = nullptr;
size_t ringSize = 0;
size_t head = 0;  // Next write position
bool wrapped = false;
#if ENABLE_SERIAL_CONSOLE
Level threshold = Level::Debug;
#else
Level threshold = Level::Info;
#endif

void appendToRing(const char *text, size_t length) {
  for (size_t i = 0; i < length; i++) {
    ring[head] = text[i];
    if (++head == ringSize) {
      head = 0;
      wrapped = true;
    }
  }
}

}  // namespace

void begin(size_t ringBytes) {
  if (mutex) return;
  mutex = xSemaphoreCreateMutex();
  ring = static_cast<char *>(heap_caps_malloc(ringBytes, MALLOC_CAP_SPIRAM));
  if (!ring) ring = static_cast<char *>(malloc(ringBytes));
  ringSize = ring ? ringBytes : 0;
}

void setLevel(Level level) { threshold = level; }
Level level() { return threshold; }

void write(Level level, const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vwrite(level, tag, format, args);
  va_end(args);
}

void vwrite(Level level, const char *tag, const char *format, va_list args) {
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(threshold)) return;

  char line[256];
  const uint32_t ms = millis();
  int length = snprintf(line, sizeof(line), "%lu.%03lu %c %s: ", static_cast<unsigned long>(ms / 1000),
                        static_cast<unsigned long>(ms % 1000), kLetters[static_cast<uint8_t>(level)], tag);
  length += vsnprintf(line + length, sizeof(line) - length, format, args);
  if (length > static_cast<int>(sizeof(line)) - 2) length = sizeof(line) - 2;  // Truncated
  line[length++] = '\n';
  line[length] = '\0';

  if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
  if (ringSize) appendToRing(line, length);
  if (mutex) xSemaphoreGive(mutex);
  writeSerial(line, length);
}

void writeSerial(const char *data, size_t length, uint32_t patienceMs) {
  // Once a write has given up, skip waiting until the reader drains the buffer,
  // so a PC that holds the port without reading costs each log line nothing.
  static bool stalled = false;
  if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
  size_t sent = 0;
  uint32_t lastProgress = millis();
  while (sent < length) {
    const int room = Serial.availableForWrite();
    if (room > 0) {
      stalled = false;
      sent += Serial.write(reinterpret_cast<const uint8_t *>(data) + sent,
                           min(static_cast<size_t>(room), length - sent));
      lastProgress = millis();
    } else if (stalled || !Serial || millis() - lastProgress >= patienceMs) {
      stalled = true;
      break;
    } else {
      delay(1);
    }
  }
  if (mutex) xSemaphoreGive(mutex);
}

size_t copyRecent(char *out, size_t capacity) {
  if (capacity == 0) return 0;
  if (!mutex || !ringSize) {
    out[0] = '\0';
    return 0;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  const size_t stored = wrapped ? ringSize : head;
  const size_t start = wrapped ? head : 0;  // Oldest byte
  size_t skip = stored > capacity - 1 ? stored - (capacity - 1) : 0;
  // Start on a line boundary if the oldest line would be cut.
  if (wrapped || skip) {
    while (skip < stored && ring[(start + skip) % ringSize] != '\n') skip++;
    if (skip < stored) skip++;
  }
  size_t n = 0;
  for (size_t i = skip; i < stored; i++) out[n++] = ring[(start + i) % ringSize];
  out[n] = '\0';
  xSemaphoreGive(mutex);
  return n;
}

}  // namespace logging
