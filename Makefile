CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Iinclude
APP_LDFLAGS := -framework CoreMIDI -framework AudioToolbox -framework CoreFoundation -framework CoreAudio

APP := midi-capture
APP_SRCS := src/main.c src/midi_parser.c src/status_line.c
TESTS := test_midi_parser test_status_line

.PHONY: all clean test

all: $(APP)

$(APP): $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $(APP_SRCS) $(APP_LDFLAGS)

test_midi_parser: tests/test_midi_parser.c src/midi_parser.c
	$(CC) $(CFLAGS) -o $@ tests/test_midi_parser.c src/midi_parser.c

test_status_line: tests/test_status_line.c src/status_line.c
	$(CC) $(CFLAGS) -o $@ tests/test_status_line.c src/status_line.c

test: $(TESTS)
	./test_midi_parser
	./test_status_line

clean:
	rm -f $(APP) $(TESTS) output.mid
