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
- It records channel voice messages, MIDI running-status channel messages, and SysEx into the MIDI file.
- It ignores MIDI clock, active sensing, and other unsupported or incomplete system messages.
- During `record`, the terminal shows `REC`, elapsed time, and an `RX.` flash when MIDI arrives.
- During `play`, the terminal shows `PLAY`, elapsed and total time, and a `TX.` flash when MIDI is sent.
