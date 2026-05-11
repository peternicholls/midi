#ifndef APP_SUPPORT_H
#define APP_SUPPORT_H

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/HostTime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern volatile sig_atomic_t g_stop_requested;

void on_sigint(int signal_number);
void log_osstatus_error(const char *label, OSStatus status);
uint64_t host_time_now_nanos(void);
bool activity_is_active(uint64_t now_nanos, uint64_t last_activity_nanos);
void draw_status_line(const char *mode, double elapsed_seconds, bool has_total,
                      double total_seconds, bool rx_active, bool tx_active);
void get_endpoint_name(MIDIEndpointRef endpoint, char *buffer,
                       size_t buffer_size);
MIDIEndpointRef get_source_by_index(long index);
MIDIEndpointRef get_destination_by_index(long index);
int parse_long_arg(const char *value, long *result);
int create_file_url(const char *path, CFURLRef *out_url);

#endif
