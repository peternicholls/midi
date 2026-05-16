#include "app_support.h"
#include "status_line.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t g_stop_requested = 0;
static const uint64_t kActivityFlashNanos = 150000000ULL;

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

static const char kAppVersion[] = APP_VERSION;

const char *app_version(void) { return kAppVersion; }

static int cfstring_to_utf8(CFStringRef value, char *buffer,
                            size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    return 0;
  }

  buffer[0] = '\0';
  if (value == NULL) {
    return 0;
  }

  return CFStringGetCString(value, buffer, buffer_size, kCFStringEncodingUTF8);
}

void on_sigint(int signal_number) {
  (void)signal_number;
  g_stop_requested = 1;
}

void log_osstatus_error(const char *label, OSStatus status) {
  fprintf(stderr, "%s failed: OSStatus=%d\n", label, (int)status);
}

uint64_t host_time_now_nanos(void) {
  return AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
}

bool activity_is_active(uint64_t now_nanos, uint64_t last_activity_nanos) {
  return last_activity_nanos != 0 && now_nanos >= last_activity_nanos &&
         (now_nanos - last_activity_nanos) <= kActivityFlashNanos;
}

void draw_status_line(const char *mode, double elapsed_seconds, bool has_total,
                      double total_seconds, bool rx_active, bool tx_active) {
  char line[128];

  format_status_line(line, sizeof(line), mode, elapsed_seconds, has_total,
                     total_seconds, rx_active, tx_active);
  printf("\r%-64s", line);
  fflush(stdout);
}

void get_endpoint_name(MIDIEndpointRef endpoint, char *buffer,
                       size_t buffer_size) {
  buffer[0] = '\0';

  CFStringRef name = NULL;
  if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) ==
          noErr &&
      name != NULL) {
    if (cfstring_to_utf8(name, buffer, buffer_size)) {
      CFRelease(name);
      return;
    }
    CFRelease(name);
  }

  snprintf(buffer, buffer_size, "endpoint-%lu", (unsigned long)endpoint);
}

MIDIEndpointRef get_source_by_index(long index) {
  const ItemCount count = MIDIGetNumberOfSources();
  if (index < 0 || (ItemCount)index >= count) {
    return 0;
  }
  return MIDIGetSource((ItemCount)index);
}

MIDIEndpointRef get_destination_by_index(long index) {
  const ItemCount count = MIDIGetNumberOfDestinations();
  if (index < 0 || (ItemCount)index >= count) {
    return 0;
  }
  return MIDIGetDestination((ItemCount)index);
}

int parse_long_arg(const char *value, long *result) {
  if (value == NULL || *value == '\0' || result == NULL) {
    return 0;
  }

  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (end == NULL || *end != '\0') {
    return 0;
  }

  *result = parsed;
  return 1;
}

int create_file_url(const char *path, CFURLRef *out_url) {
  if (path == NULL || out_url == NULL) {
    return 0;
  }

  *out_url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8 *)path, (CFIndex)strlen(path), false);
  return *out_url != NULL;
}
