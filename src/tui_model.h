#ifndef TUI_MODEL_H
#define TUI_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

int tui_format_recording_name(const struct tm *local_time, char *buffer,
                              size_t buffer_size);
void tui_format_midi_bytes(char *buffer, size_t buffer_size,
                           const uint8_t *bytes, size_t length);
void tui_format_clock_time(char *buffer, size_t buffer_size, double seconds);

#endif
