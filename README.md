# MIDI Capture

Small macOS command-line tool in C that:

- lists available CoreMIDI sources and destinations
- records MIDI from a class-compliant USB MIDI interface into a Standard MIDI File
- plays that `.mid` file back to a chosen MIDI destination

## Build

```sh
make
```

## Test

```sh
make test
```

## Usage

List MIDI ports:

```sh
./midi-capture list
```

Record from source `0` for 10 seconds:

```sh
./midi-capture record take.mid 10 0
```

Record until `Ctrl-C`:

```sh
./midi-capture record take.mid
```

Play the file back to destination `0`:

```sh
./midi-capture play take.mid 0
```

## Notes

- The tool is intentionally small and targets MIDI 1.0 byte-stream devices on macOS.
- It records channel voice messages and SysEx into the MIDI file.
- It ignores MIDI clock, active sensing, and other unsupported or incomplete system messages.
- During `record`, the terminal shows `REC`, elapsed time, and an `RX.` flash when MIDI arrives.
- During `play`, the terminal shows `PLAY`, elapsed and total time, and a `TX.` flash when MIDI is sent.

Built a small macOS CLI recorder/player in C. The main CoreMIDI/AudioToolbox flow is in src/main.c:31, MIDI byte
  parsing is in src/midi_parser.c:1, the parser regression tests are in tests/test_midi_parser.c:1, and build/run
  instructions are in README.md:1. The binary is midi-capture with list, record, and play commands.

  Simplifications made: it records MIDI 1.0 channel voice messages and SysEx into a standard .mid file, ignores clock/
  active-sensing/other unsupported system messages, defaults to source/destination 0 unless you pass an index, and uses
  MIDIInputPortCreate to keep the capture path as plain C byte-stream code.

  Verification: make test passed, make passed, and ./midi-capture list ran successfully. On this machine it reported no
  MIDI endpoints, so actual record/playback against attached hardware is the main remaining risk, along with unsupported
  running-status or split-packet SysEx edge cases.

  Apple API refs used: MIDIInputPortCreate
  (https://developer.apple.com/documentation/coremidi/midiinputportcreate%28_%3A_%3A_%3A_%3A_%3A%29?language=objc), Mus
  icSequenceFileCreate
  (https://developer.apple.com/documentation/audiotoolbox/1502760-musicsequencefilecreate?changes=la__3__8&language=objc),
  MusicSequenceSetMIDIEndpoint
  (https://developer.apple.com/documentation/audiotoolbox/musicsequencesetmidiendpoint%28_%3A_%3A%29), MusicTrackNewMID
  IChannelEvent
  (https://developer.apple.com/documentation/audiotoolbox/musictracknewmidichannelevent%28_%3A_%3A_%3A%29), MusicSequen
  ceGetBeatsForSeconds
  (https://developer.apple.com/documentation/audiotoolbox/musicsequencegetbeatsforseconds%28_%3A_%3A_%3A%29?language=objc).
