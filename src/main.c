#include "midi_parser.h"
#include "status_line.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/HostTime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct AppRecorderState {
    MusicSequence sequence;
    MusicTrack track;
    uint64_t first_host_time;
    bool has_first_host_time;
    atomic_uint captured_events;
    atomic_uint ignored_messages;
    atomic_ullong last_rx_nanos;
    MidiParserState parser_state;
} AppRecorderState;

static volatile sig_atomic_t g_stop_requested = 0;
static const uint64_t kActivityFlashNanos = 150000000ULL;

typedef struct PlaybackEvent {
    double seconds;
    size_t length;
    uint8_t inline_bytes[3];
    uint8_t *data;
    unsigned long order;
} PlaybackEvent;

typedef struct PlaybackEventList {
    PlaybackEvent *items;
    size_t count;
    size_t capacity;
    unsigned long next_order;
} PlaybackEventList;

static OSStatus sequence_length_in_beats(MusicSequence sequence, MusicTimeStamp *out_beats);

static void on_sigint(int signal_number) {
    (void)signal_number;
    g_stop_requested = 1;
}

static void print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage:\n"
            "  %s list\n"
            "  %s record <output.mid> [seconds] [source-index]\n"
            "  %s play <input.mid> [destination-index]\n",
            program_name,
            program_name,
            program_name);
}

static void log_osstatus_error(const char *label, OSStatus status) {
    fprintf(stderr, "%s failed: OSStatus=%d\n", label, (int)status);
}

static uint64_t host_time_now_nanos(void) {
    return AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
}

static bool activity_is_active(uint64_t now_nanos, uint64_t last_activity_nanos) {
    return last_activity_nanos != 0 &&
           now_nanos >= last_activity_nanos &&
           (now_nanos - last_activity_nanos) <= kActivityFlashNanos;
}

static void draw_status_line(const char *mode,
                             double elapsed_seconds,
                             bool has_total,
                             double total_seconds,
                             bool rx_active,
                             bool tx_active) {
    char line[128];
    format_status_line(line,
                       sizeof(line),
                       mode,
                       elapsed_seconds,
                       has_total,
                       total_seconds,
                       rx_active,
                       tx_active);
    printf("\r%-64s", line);
    fflush(stdout);
}

static int cfstring_to_utf8(CFStringRef value, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    buffer[0] = '\0';
    if (value == NULL) {
        return 0;
    }

    return CFStringGetCString(value, buffer, buffer_size, kCFStringEncodingUTF8);
}

static void get_endpoint_name(MIDIEndpointRef endpoint, char *buffer, size_t buffer_size) {
    buffer[0] = '\0';

    CFStringRef name = NULL;
    if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) == noErr && name != NULL) {
        if (cfstring_to_utf8(name, buffer, buffer_size)) {
            CFRelease(name);
            return;
        }
        CFRelease(name);
    }

    snprintf(buffer, buffer_size, "endpoint-%lu", (unsigned long)endpoint);
}

static MIDIEndpointRef get_source_by_index(long index) {
    const ItemCount count = MIDIGetNumberOfSources();
    if (index < 0 || (ItemCount)index >= count) {
        return 0;
    }
    return MIDIGetSource((ItemCount)index);
}

static MIDIEndpointRef get_destination_by_index(long index) {
    const ItemCount count = MIDIGetNumberOfDestinations();
    if (index < 0 || (ItemCount)index >= count) {
        return 0;
    }
    return MIDIGetDestination((ItemCount)index);
}

static int parse_long_arg(const char *value, long *result) {
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

static int create_file_url(const char *path, CFURLRef *out_url) {
    if (path == NULL || out_url == NULL) {
        return 0;
    }

    *out_url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                       (const UInt8 *)path,
                                                       (CFIndex)strlen(path),
                                                       false);
    return *out_url != NULL;
}

static int add_channel_event(AppRecorderState *state,
                             const uint8_t *bytes,
                             size_t length,
                             MusicTimeStamp beats) {
    if (state == NULL || bytes == NULL || length < 2) {
        return 0;
    }

    MIDIChannelMessage message;
    memset(&message, 0, sizeof(message));
    message.status = bytes[0];
    message.data1 = length > 1 ? bytes[1] : 0;
    message.data2 = length > 2 ? bytes[2] : 0;

    OSStatus status = MusicTrackNewMIDIChannelEvent(state->track, beats, &message);
    if (status != noErr) {
        log_osstatus_error("MusicTrackNewMIDIChannelEvent", status);
        return 0;
    }

    atomic_fetch_add_explicit(&state->captured_events, 1u, memory_order_relaxed);
    return 1;
}

static int add_sysex_event(AppRecorderState *state,
                           const uint8_t *bytes,
                           size_t length,
                           MusicTimeStamp beats) {
    if (state == NULL || bytes == NULL || length == 0) {
        return 0;
    }

    MIDIRawData *raw = (MIDIRawData *)malloc(sizeof(MIDIRawData) + length - 1);
    if (raw == NULL) {
        fputs("out of memory while recording SysEx\n", stderr);
        return 0;
    }

    raw->length = (UInt32)length;
    memcpy(raw->data, bytes, length);

    OSStatus status = MusicTrackNewMIDIRawDataEvent(state->track, beats, raw);
    free(raw);
    if (status != noErr) {
        log_osstatus_error("MusicTrackNewMIDIRawDataEvent", status);
        return 0;
    }

    atomic_fetch_add_explicit(&state->captured_events, 1u, memory_order_relaxed);
    return 1;
}

static void record_packet_bytes(AppRecorderState *state,
                                const uint8_t *bytes,
                                size_t byte_count,
                                uint64_t host_time) {
    if (state == NULL || bytes == NULL || byte_count == 0) {
        return;
    }

    if (!state->has_first_host_time) {
        state->first_host_time = host_time;
        state->has_first_host_time = true;
    }

    atomic_store_explicit(&state->last_rx_nanos,
                          AudioConvertHostTimeToNanos(host_time),
                          memory_order_relaxed);

    double elapsed_seconds = 0.0;
    if (host_time >= state->first_host_time) {
        elapsed_seconds = (double)AudioConvertHostTimeToNanos(host_time - state->first_host_time) / 1000000000.0;
    }

    MusicTimeStamp beats = 0;
    OSStatus status = MusicSequenceGetBeatsForSeconds(state->sequence, elapsed_seconds, &beats);
    if (status != noErr) {
        log_osstatus_error("MusicSequenceGetBeatsForSeconds", status);
        return;
    }

    size_t offset = 0;
    while (offset < byte_count) {
        MidiParsedMessage parsed;
        size_t used = midi_next_stream_message(&state->parser_state, bytes + offset, byte_count - offset, &parsed);
        if (used == 0) {
            atomic_fetch_add_explicit(&state->ignored_messages, 1u, memory_order_relaxed);
            break;
        }

        switch (parsed.kind) {
            case MIDI_PARSED_CHANNEL:
                if (!add_channel_event(state, parsed.channel_bytes, parsed.channel_length, beats)) {
                    g_stop_requested = 1;
                    return;
                }
                break;
            case MIDI_PARSED_SYSEX:
                if (!add_sysex_event(state, bytes + offset, parsed.length, beats)) {
                    g_stop_requested = 1;
                    return;
                }
                break;
            case MIDI_PARSED_UNSUPPORTED:
                atomic_fetch_add_explicit(&state->ignored_messages, 1u, memory_order_relaxed);
                break;
            case MIDI_PARSED_INCOMPLETE:
                atomic_fetch_add_explicit(&state->ignored_messages, 1u, memory_order_relaxed);
                return;
        }

        offset += used;
    }
}

static void free_playback_events(PlaybackEventList *events) {
    if (events == NULL) {
        return;
    }

    for (size_t i = 0; i < events->count; ++i) {
        if (events->items[i].data != NULL && events->items[i].data != events->items[i].inline_bytes) {
            free(events->items[i].data);
        }
    }

    free(events->items);
    events->items = NULL;
    events->count = 0;
    events->capacity = 0;
    events->next_order = 0;
}

static int append_playback_event(PlaybackEventList *events,
                                 double seconds,
                                 const uint8_t *data,
                                 size_t length) {
    if (events == NULL || data == NULL || length == 0) {
        return 0;
    }

    if (events->count == events->capacity) {
        size_t new_capacity = events->capacity == 0 ? 32 : events->capacity * 2;
        PlaybackEvent *resized = (PlaybackEvent *)realloc(events->items, new_capacity * sizeof(*resized));
        if (resized == NULL) {
            fputs("out of memory while preparing playback events\n", stderr);
            return 0;
        }
        events->items = resized;
        events->capacity = new_capacity;
    }

    PlaybackEvent *event = &events->items[events->count];
    memset(event, 0, sizeof(*event));
    event->seconds = seconds;
    event->length = length;

    if (length <= sizeof(event->inline_bytes)) {
        memcpy(event->inline_bytes, data, length);
        event->data = event->inline_bytes;
    } else {
        event->data = (uint8_t *)malloc(length);
        if (event->data == NULL) {
            fputs("out of memory while copying playback bytes\n", stderr);
            return 0;
        }
        memcpy(event->data, data, length);
    }

    event->order = events->next_order++;
    events->count += 1;
    return 1;
}

static int compare_playback_events(const void *left, const void *right) {
    const PlaybackEvent *a = (const PlaybackEvent *)left;
    const PlaybackEvent *b = (const PlaybackEvent *)right;

    if (a->seconds < b->seconds) {
        return -1;
    }
    if (a->seconds > b->seconds) {
        return 1;
    }
    if (a->order < b->order) {
        return -1;
    }
    if (a->order > b->order) {
        return 1;
    }
    return 0;
}

static int seconds_for_beats(MusicSequence sequence, MusicTimeStamp beats, double *out_seconds) {
    OSStatus status = MusicSequenceGetSecondsForBeats(sequence, beats, out_seconds);
    if (status != noErr) {
        log_osstatus_error("MusicSequenceGetSecondsForBeats", status);
        return 0;
    }
    return 1;
}

static int collect_track_playback_events(MusicSequence sequence,
                                         MusicTrack track,
                                         PlaybackEventList *events) {
    MusicEventIterator iterator = NULL;
    OSStatus status = NewMusicEventIterator(track, &iterator);
    if (status != noErr) {
        log_osstatus_error("NewMusicEventIterator", status);
        return 0;
    }

    Boolean has_current = false;
    status = MusicEventIteratorHasCurrentEvent(iterator, &has_current);
    if (status != noErr) {
        log_osstatus_error("MusicEventIteratorHasCurrentEvent", status);
        DisposeMusicEventIterator(iterator);
        return 0;
    }

    while (has_current) {
        MusicTimeStamp beats = 0;
        MusicEventType event_type = 0;
        const void *event_data = NULL;
        UInt32 event_size = 0;
        status = MusicEventIteratorGetEventInfo(iterator, &beats, &event_type, &event_data, &event_size);
        if (status != noErr) {
            log_osstatus_error("MusicEventIteratorGetEventInfo", status);
            DisposeMusicEventIterator(iterator);
            return 0;
        }
        /* Known event structs carry their own fixed fields or embedded raw length. */
        (void)event_size;

        double seconds = 0.0;
        if (!seconds_for_beats(sequence, beats, &seconds)) {
            DisposeMusicEventIterator(iterator);
            return 0;
        }

        if (event_type == kMusicEventType_MIDIChannelMessage) {
            const MIDIChannelMessage *message = (const MIDIChannelMessage *)event_data;
            uint8_t bytes[3] = {message->status, message->data1, message->data2};
            size_t length = midi_channel_message_length(message->status);
            if (!append_playback_event(events, seconds, bytes, length)) {
                DisposeMusicEventIterator(iterator);
                return 0;
            }
        } else if (event_type == kMusicEventType_MIDIRawData) {
            const MIDIRawData *raw = (const MIDIRawData *)event_data;
            if (!append_playback_event(events, seconds, raw->data, raw->length)) {
                DisposeMusicEventIterator(iterator);
                return 0;
            }
        } else if (event_type == kMusicEventType_MIDINoteMessage) {
            const MIDINoteMessage *note = (const MIDINoteMessage *)event_data;
            uint8_t note_on[3] = {
                (uint8_t)(0x90 | (note->channel & 0x0F)),
                note->note,
                note->velocity
            };
            uint8_t note_off[3] = {
                (uint8_t)(0x80 | (note->channel & 0x0F)),
                note->note,
                note->releaseVelocity
            };
            double end_seconds = seconds;

            if (!append_playback_event(events, seconds, note_on, sizeof(note_on))) {
                DisposeMusicEventIterator(iterator);
                return 0;
            }
            if (!seconds_for_beats(sequence, beats + note->duration, &end_seconds)) {
                DisposeMusicEventIterator(iterator);
                return 0;
            }
            if (!append_playback_event(events, end_seconds, note_off, sizeof(note_off))) {
                DisposeMusicEventIterator(iterator);
                return 0;
            }
        }

        status = MusicEventIteratorNextEvent(iterator);
        if (status != noErr) {
            log_osstatus_error("MusicEventIteratorNextEvent", status);
            DisposeMusicEventIterator(iterator);
            return 0;
        }
        status = MusicEventIteratorHasCurrentEvent(iterator, &has_current);
        if (status != noErr) {
            log_osstatus_error("MusicEventIteratorHasCurrentEvent", status);
            DisposeMusicEventIterator(iterator);
            return 0;
        }
    }

    DisposeMusicEventIterator(iterator);
    return 1;
}

static int collect_playback_events(MusicSequence sequence,
                                   PlaybackEventList *events,
                                   double *out_total_seconds) {
    UInt32 track_count = 0;
    OSStatus status = MusicSequenceGetTrackCount(sequence, &track_count);
    if (status != noErr) {
        log_osstatus_error("MusicSequenceGetTrackCount", status);
        return 0;
    }

    for (UInt32 i = 0; i < track_count; ++i) {
        MusicTrack track = NULL;
        status = MusicSequenceGetIndTrack(sequence, i, &track);
        if (status != noErr) {
            log_osstatus_error("MusicSequenceGetIndTrack", status);
            return 0;
        }
        if (!collect_track_playback_events(sequence, track, events)) {
            return 0;
        }
    }

    if (events->count > 1) {
        qsort(events->items, events->count, sizeof(events->items[0]), compare_playback_events);
    }

    MusicTimeStamp end_beats = 0;
    status = sequence_length_in_beats(sequence, &end_beats);
    if (status != noErr) {
        log_osstatus_error("sequence_length_in_beats", status);
        return 0;
    }

    double total_seconds = 0.0;
    if (!seconds_for_beats(sequence, end_beats, &total_seconds)) {
        return 0;
    }
    if (events->count > 0 && events->items[events->count - 1].seconds > total_seconds) {
        total_seconds = events->items[events->count - 1].seconds;
    }
    *out_total_seconds = total_seconds;
    return 1;
}

static int send_midi_bytes(MIDIPortRef output_port,
                           MIDIEndpointRef destination,
                           const uint8_t *bytes,
                           size_t length) {
    if (bytes == NULL || length == 0) {
        return 0;
    }

    uint8_t stack_buffer[sizeof(MIDIPacketList) + 256];
    size_t packet_list_size = sizeof(MIDIPacketList) + length;
    MIDIPacketList *packet_list = (MIDIPacketList *)stack_buffer;
    bool heap_allocated = false;

    if (length > 256) {
        packet_list = (MIDIPacketList *)malloc(packet_list_size);
        if (packet_list == NULL) {
            fputs("out of memory while sending MIDI\n", stderr);
            return 0;
        }
        heap_allocated = true;
    }

    MIDIPacket *packet = MIDIPacketListInit(packet_list);
    packet = MIDIPacketListAdd(packet_list, packet_list_size, packet, 0, length, bytes);
    if (packet == NULL) {
        if (heap_allocated) {
            free(packet_list);
        }
        fputs("failed to build MIDI packet list\n", stderr);
        return 0;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    OSStatus status = MIDISend(output_port, destination, packet_list);
#pragma clang diagnostic pop

    if (heap_allocated) {
        free(packet_list);
    }
    if (status != noErr) {
        log_osstatus_error("MIDISend", status);
        return 0;
    }

    return 1;
}

static void wait_with_status(const char *mode,
                             uint64_t started_nanos,
                             double target_seconds,
                             bool has_total,
                             double total_seconds,
                             uint64_t last_rx_nanos,
                             uint64_t last_tx_nanos) {
    while (!g_stop_requested) {
        uint64_t now_nanos = host_time_now_nanos();
        double elapsed_seconds = (double)(now_nanos - started_nanos) / 1000000000.0;
        if (elapsed_seconds >= target_seconds) {
            return;
        }

        draw_status_line(mode,
                         elapsed_seconds,
                         has_total,
                         total_seconds,
                         activity_is_active(now_nanos, last_rx_nanos),
                         activity_is_active(now_nanos, last_tx_nanos));
        usleep(20000);
    }
}

static void midi_read_proc(const MIDIPacketList *packet_list,
                           void *read_proc_ref_con,
                           void *src_conn_ref_con) {
    (void)src_conn_ref_con;

    AppRecorderState *state = (AppRecorderState *)read_proc_ref_con;
    if (state == NULL || packet_list == NULL) {
        return;
    }

    const MIDIPacket *packet = &packet_list->packet[0];
    for (UInt32 i = 0; i < packet_list->numPackets; ++i) {
        const uint64_t packet_time = packet->timeStamp != 0 ? packet->timeStamp : AudioGetCurrentHostTime();
        record_packet_bytes(state, packet->data, packet->length, packet_time);
        packet = MIDIPacketNext(packet);
    }
}

static OSStatus save_sequence_to_file(MusicSequence sequence, const char *output_path) {
    CFURLRef url = NULL;
    if (!create_file_url(output_path, &url)) {
        fputs("could not create output URL\n", stderr);
        return -50;
    }

    OSStatus status = MusicSequenceFileCreate(sequence,
                                              url,
                                              kMusicSequenceFile_MIDIType,
                                              kMusicSequenceFileFlags_EraseFile,
                                              480);
    CFRelease(url);
    return status;
}

static int command_list(void) {
    MIDIClientRef client = 0;
    OSStatus status = MIDIClientCreate(CFSTR("midi-capture-list-client"), NULL, NULL, &client);
    if (status != noErr) {
        client = 0;
    }

    const ItemCount source_count = MIDIGetNumberOfSources();
    const ItemCount destination_count = MIDIGetNumberOfDestinations();
    char name[256];

    puts("Sources:");
    for (ItemCount i = 0; i < source_count; ++i) {
        get_endpoint_name(MIDIGetSource(i), name, sizeof(name));
        printf("  [%lu] %s\n", (unsigned long)i, name);
    }

    puts("Destinations:");
    for (ItemCount i = 0; i < destination_count; ++i) {
        get_endpoint_name(MIDIGetDestination(i), name, sizeof(name));
        printf("  [%lu] %s\n", (unsigned long)i, name);
    }

    if (source_count == 0 && destination_count == 0) {
        puts("  no MIDI endpoints found");
    }

    if (client != 0) {
        MIDIClientDispose(client);
    }
    return 0;
}

static int command_record(const char *output_path, const char *seconds_arg, const char *source_arg) {
    long requested_seconds = -1;
    long source_index = 0;
    if (seconds_arg != NULL && !parse_long_arg(seconds_arg, &requested_seconds)) {
        fputs("seconds must be an integer\n", stderr);
        return 1;
    }
    if (source_arg != NULL && !parse_long_arg(source_arg, &source_index)) {
        fputs("source-index must be an integer\n", stderr);
        return 1;
    }
    if (requested_seconds == 0 || requested_seconds < -1) {
        fputs("seconds must be positive, or omit it to record until Ctrl-C\n", stderr);
        return 1;
    }

    MIDIEndpointRef source = get_source_by_index(source_index);
    if (source == 0) {
        fputs("invalid source index or no MIDI sources are available\n", stderr);
        return 1;
    }

    char source_name[256];
    get_endpoint_name(source, source_name, sizeof(source_name));

    MIDIClientRef client = 0;
    MIDIPortRef input_port = 0;
    MusicSequence sequence = NULL;
    MusicTrack track = NULL;
    AppRecorderState state;
    memset(&state, 0, sizeof(state));
    midi_parser_state_init(&state.parser_state);

    OSStatus status = NewMusicSequence(&sequence);
    if (status != noErr) {
        log_osstatus_error("NewMusicSequence", status);
        return 1;
    }

    status = MusicSequenceNewTrack(sequence, &track);
    if (status != noErr) {
        log_osstatus_error("MusicSequenceNewTrack", status);
        DisposeMusicSequence(sequence);
        return 1;
    }

    state.sequence = sequence;
    state.track = track;

    status = MIDIClientCreate(CFSTR("midi-capture-client"), NULL, NULL, &client);
    if (status != noErr) {
        log_osstatus_error("MIDIClientCreate", status);
        DisposeMusicSequence(sequence);
        return 1;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    status = MIDIInputPortCreate(client, CFSTR("midi-capture-input"), midi_read_proc, &state, &input_port);
#pragma clang diagnostic pop
    if (status != noErr) {
        log_osstatus_error("MIDIInputPortCreate", status);
        MIDIClientDispose(client);
        DisposeMusicSequence(sequence);
        return 1;
    }

    status = MIDIPortConnectSource(input_port, source, NULL);
    if (status != noErr) {
        log_osstatus_error("MIDIPortConnectSource", status);
        MIDIPortDispose(input_port);
        MIDIClientDispose(client);
        DisposeMusicSequence(sequence);
        return 1;
    }

    g_stop_requested = 0;
    signal(SIGINT, on_sigint);

    if (requested_seconds > 0) {
        printf("Recording from source [%ld] %s for %ld second(s)...\n",
               source_index,
               source_name,
               requested_seconds);
    } else {
        printf("Recording from source [%ld] %s until Ctrl-C...\n", source_index, source_name);
    }

    const uint64_t started_nanos = host_time_now_nanos();
    while (!g_stop_requested) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
        uint64_t now_nanos = host_time_now_nanos();
        double elapsed_seconds = (double)(now_nanos - started_nanos) / 1000000000.0;
        draw_status_line("REC",
                         elapsed_seconds,
                         false,
                         0.0,
                         activity_is_active(now_nanos,
                                            atomic_load_explicit(&state.last_rx_nanos, memory_order_relaxed)),
                         false);
        if (requested_seconds > 0 && elapsed_seconds >= (double)requested_seconds) {
            break;
        }
    }

    status = save_sequence_to_file(sequence, output_path);
    if (status != noErr) {
        log_osstatus_error("MusicSequenceFileCreate", status);
        MIDIPortDispose(input_port);
        MIDIClientDispose(client);
        DisposeMusicSequence(sequence);
        return 1;
    }

    putchar('\n');
    MIDIPortDispose(input_port);
    MIDIClientDispose(client);
    DisposeMusicSequence(sequence);

    unsigned int captured_events = atomic_load_explicit(&state.captured_events, memory_order_relaxed);
    unsigned int ignored_messages = atomic_load_explicit(&state.ignored_messages, memory_order_relaxed);
    printf("Saved %u event(s) to %s", captured_events, output_path);
    if (ignored_messages > 0) {
        printf(" (%u unsupported/incomplete message(s) ignored)", ignored_messages);
    }
    putchar('\n');
    return 0;
}

static OSStatus sequence_length_in_beats(MusicSequence sequence, MusicTimeStamp *out_beats) {
    UInt32 track_count = 0;
    OSStatus status = MusicSequenceGetTrackCount(sequence, &track_count);
    if (status != noErr) {
        return status;
    }

    MusicTimeStamp max_beats = 0;
    for (UInt32 i = 0; i < track_count; ++i) {
        MusicTrack track = NULL;
        status = MusicSequenceGetIndTrack(sequence, i, &track);
        if (status != noErr) {
            return status;
        }

        MusicTimeStamp track_length = 0;
        UInt32 property_size = (UInt32)sizeof(track_length);
        status = MusicTrackGetProperty(track,
                                       kSequenceTrackProperty_TrackLength,
                                       &track_length,
                                       &property_size);
        if (status != noErr) {
            return status;
        }

        if (track_length > max_beats) {
            max_beats = track_length;
        }
    }

    *out_beats = max_beats;
    return noErr;
}

static int command_play(const char *input_path, const char *destination_arg) {
    long destination_index = 0;
    if (destination_arg != NULL && !parse_long_arg(destination_arg, &destination_index)) {
        fputs("destination-index must be an integer\n", stderr);
        return 1;
    }

    MIDIEndpointRef destination = get_destination_by_index(destination_index);
    if (destination == 0) {
        fputs("invalid destination index or no MIDI destinations are available\n", stderr);
        return 1;
    }

    char destination_name[256];
    get_endpoint_name(destination, destination_name, sizeof(destination_name));

    CFURLRef url = NULL;
    if (!create_file_url(input_path, &url)) {
        fputs("could not create input URL\n", stderr);
        return 1;
    }

    MusicSequence sequence = NULL;
    MIDIClientRef client = 0;
    MIDIPortRef output_port = 0;
    PlaybackEventList events;
    memset(&events, 0, sizeof(events));

    OSStatus status = NewMusicSequence(&sequence);
    if (status != noErr) {
        CFRelease(url);
        log_osstatus_error("NewMusicSequence", status);
        return 1;
    }

    status = MusicSequenceFileLoad(sequence, url, kMusicSequenceFile_AnyType, 0);
    CFRelease(url);
    if (status != noErr) {
        log_osstatus_error("MusicSequenceFileLoad", status);
        DisposeMusicSequence(sequence);
        return 1;
    }

    double total_seconds = 0.0;
    if (!collect_playback_events(sequence, &events, &total_seconds)) {
        free_playback_events(&events);
        DisposeMusicSequence(sequence);
        return 1;
    }

    status = MIDIClientCreate(CFSTR("midi-capture-playback-client"), NULL, NULL, &client);
    if (status != noErr) {
        log_osstatus_error("MIDIClientCreate", status);
        free_playback_events(&events);
        DisposeMusicSequence(sequence);
        return 1;
    }

    status = MIDIOutputPortCreate(client, CFSTR("midi-capture-output"), &output_port);
    if (status != noErr) {
        log_osstatus_error("MIDIOutputPortCreate", status);
        MIDIClientDispose(client);
        free_playback_events(&events);
        DisposeMusicSequence(sequence);
        return 1;
    }

    g_stop_requested = 0;
    signal(SIGINT, on_sigint);

    printf("Playing %s to destination [%ld] %s...\n", input_path, destination_index, destination_name);
    uint64_t started_nanos = host_time_now_nanos();
    uint64_t last_tx_nanos = 0;
    for (size_t i = 0; i < events.count && !g_stop_requested; ++i) {
        wait_with_status("PLAY",
                         started_nanos,
                         events.items[i].seconds,
                         true,
                         total_seconds,
                         0,
                         last_tx_nanos);
        if (g_stop_requested) {
            break;
        }
        if (!send_midi_bytes(output_port,
                             destination,
                             events.items[i].data,
                             events.items[i].length)) {
            MIDIPortDispose(output_port);
            MIDIClientDispose(client);
            free_playback_events(&events);
            DisposeMusicSequence(sequence);
            return 1;
        }
        last_tx_nanos = host_time_now_nanos();
        draw_status_line("PLAY",
                         events.items[i].seconds,
                         true,
                         total_seconds,
                         false,
                         true);
    }

    wait_with_status("PLAY",
                     started_nanos,
                     total_seconds,
                     true,
                     total_seconds,
                     0,
                     last_tx_nanos);
    draw_status_line("PLAY", total_seconds, true, total_seconds, false, false);
    putchar('\n');

    MIDIPortDispose(output_port);
    MIDIClientDispose(client);
    free_playback_events(&events);
    DisposeMusicSequence(sequence);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        return command_list();
    }

    if (strcmp(argv[1], "record") == 0) {
        if (argc < 3 || argc > 5) {
            print_usage(argv[0]);
            return 1;
        }
        return command_record(argv[2], argc > 3 ? argv[3] : NULL, argc > 4 ? argv[4] : NULL);
    }

    if (strcmp(argv[1], "play") == 0) {
        if (argc < 3 || argc > 4) {
            print_usage(argv[0]);
            return 1;
        }
        return command_play(argv[2], argc > 3 ? argv[3] : NULL);
    }

    print_usage(argv[0]);
    return 1;
}
