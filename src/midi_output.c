#include "midi_output.h"

#include <stdbool.h>
#include <stdlib.h>

void midi_output_init(MidiOutput *output) {
  if (output == NULL) {
    return;
  }
  output->client = 0;
  output->port = 0;
}

MidiResult midi_output_open(MidiOutput *output, CFStringRef client_name,
                            CFStringRef port_name) {
  OSStatus status;

  if (output == NULL || client_name == NULL || port_name == NULL) {
    return midi_result_invalid_argument("midi_output_open");
  }

  midi_output_close(output);

  status = MIDIClientCreate(client_name, NULL, NULL, &output->client);
  if (status != noErr) {
    midi_output_init(output);
    return midi_result_osstatus("MIDIClientCreate", status);
  }

  status = MIDIOutputPortCreate(output->client, port_name, &output->port);
  if (status != noErr) {
    midi_output_close(output);
    return midi_result_osstatus("MIDIOutputPortCreate", status);
  }

  return midi_result_ok();
}

void midi_output_close(MidiOutput *output) {
  if (output == NULL) {
    return;
  }
  if (output->port != 0) {
    MIDIPortDispose(output->port);
    output->port = 0;
  }
  if (output->client != 0) {
    MIDIClientDispose(output->client);
    output->client = 0;
  }
}

MidiResult midi_output_send(MidiOutput *output, MIDIEndpointRef destination,
                            const uint8_t *bytes, size_t length) {
  uint8_t stack_buffer[sizeof(MIDIPacketList) + 256];
  size_t packet_list_size = sizeof(MIDIPacketList) + length;
  MIDIPacketList *packet_list = (MIDIPacketList *)stack_buffer;
  MIDIPacket *packet;
  bool heap_allocated = false;
  OSStatus status;

  if (output == NULL || output->port == 0 || destination == 0 ||
      bytes == NULL || length == 0) {
    return midi_result_invalid_argument("midi_output_send");
  }

  if (length > 256) {
    packet_list = (MIDIPacketList *)malloc(packet_list_size);
    if (packet_list == NULL) {
      return midi_result_no_memory("midi_output_send");
    }
    heap_allocated = true;
  }

  packet = MIDIPacketListInit(packet_list);
  packet = MIDIPacketListAdd(packet_list, packet_list_size, packet, 0, length,
                             bytes);
  if (packet == NULL) {
    if (heap_allocated) {
      free(packet_list);
    }
    return midi_result_invalid_argument("MIDIPacketListAdd");
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  status = MIDISend(output->port, destination, packet_list);
#pragma clang diagnostic pop

  if (heap_allocated) {
    free(packet_list);
  }
  if (status != noErr) {
    return midi_result_osstatus("MIDISend", status);
  }

  return midi_result_ok();
}
