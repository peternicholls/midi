#include "command_list.h"

#include "app_support.h"

#include <stdio.h>

int command_list(void) {
  MIDIClientRef client = 0;
  OSStatus status =
      MIDIClientCreate(CFSTR("midi-capture-list-client"), NULL, NULL, &client);
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
