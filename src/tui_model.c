#include "tui_model.h"

#include <stdio.h>

int tui_format_recording_name(const struct tm *local_time, char *buffer,
                              size_t buffer_size) {
  if (local_time == NULL || buffer == NULL || buffer_size == 0) {
    return 0;
  }

  return strftime(buffer, buffer_size, "%Y%m%d%H%M%S.mid", local_time) > 0;
}

void tui_format_midi_bytes(char *buffer, size_t buffer_size,
                           const uint8_t *bytes, size_t length) {
  size_t offset = 0;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  buffer[0] = '\0';
  if (bytes == NULL || length == 0) {
    return;
  }

  for (size_t i = 0; i < length; ++i) {
    int written = snprintf(buffer + offset, buffer_size - offset,
                           i == 0 ? "%02X" : " %02X", bytes[i]);
    if (written < 0 || (size_t)written >= buffer_size - offset) {
      return;
    }
    offset += (size_t)written;
  }
}

void tui_format_clock_time(char *buffer, size_t buffer_size, double seconds) {
  int whole_minutes;
  double remaining_seconds;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  if (seconds < 0.0) {
    seconds = 0.0;
  }
  whole_minutes = (int)(seconds / 60.0);
  remaining_seconds = seconds - (double)(whole_minutes * 60);
  snprintf(buffer, buffer_size, "%02d:%04.1f", whole_minutes,
           remaining_seconds);
}
