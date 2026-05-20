# MIDI Capture

MIDI Capture is a small macOS terminal app for recording, inspecting, and
playing back MIDI 1.0 data.

Use it when you want a simple way to:

- see which MIDI sources and destinations macOS can currently detect
- record hardware or virtual MIDI output into a Standard MIDI File
- play a `.mid` file back to a CoreMIDI destination
- browse takes from the terminal and watch live incoming MIDI activity

## Why Use It

MIDI tools are often bundled into large DAWs or graphical utilities. MIDI
Capture is deliberately smaller: it focuses on quick capture, playback, and
inspection from the command line.

That makes it useful for:

- checking whether a USB MIDI device is visible to macOS
- capturing a short performance or test pattern without opening a DAW
- replaying a MIDI file into a synth, sampler, or virtual MIDI destination
- debugging live MIDI traffic while developing or configuring a setup
- keeping timestamped MIDI takes in a plain folder

## Where It Runs

MIDI Capture currently runs on macOS. It uses Apple CoreMIDI and targets MIDI
1.0 byte-stream devices, including class-compliant USB MIDI interfaces and
virtual MIDI endpoints.

The app is terminal-based. You can use it as one-shot commands, or launch the
TUI for an interactive recordings browser.

## Quick Start

Build the app:

```sh
make
```

List available MIDI ports:

```sh
./midi list
```

Record MIDI from source `0` for 10 seconds:

```sh
./midi record take.mid 10 0
```

Play that file back to destination `0`:

```sh
./midi play take.mid 0
```

Open the terminal UI:

```sh
./midi tui recordings
```

## Common Workflows

### Check A MIDI Device

Connect the device, then run:

```sh
./midi list
```

If macOS exposes the device through CoreMIDI, it appears under `Sources`,
`Destinations`, or both.

### Capture A Take

Record to a named file for a fixed duration:

```sh
./midi record take.mid 10 0
```

Record until you press `Ctrl-C`:

```sh
./midi record take.mid
```

During recording, the terminal shows elapsed time and an `RX.` flash when MIDI
arrives.

### Play Back A MIDI File

Send a `.mid` file to a destination:

```sh
./midi play take.mid 0
```

During playback, the terminal shows elapsed time, total time, and a `TX.` flash
when MIDI is sent.

### Browse And Record In The TUI

Launch the TUI with a dedicated recordings folder:

```sh
./midi tui recordings
```

The TUI shows your recordings, the MIDI events in the selected file, and live
incoming MIDI activity. New recordings use timestamped filenames like
`YYYYMMDDhhmmss.mid` in the selected recordings directory.

Useful keys:

- `r` starts recording to a new timestamped `.mid` file
- `s` stops recording or playback
- `space` pauses or resumes recording or playback
- `p` plays the selected file from the selected MIDI event
- `up` and `down` choose a recording
- `left` and `right` step through MIDI events
- `o` changes the recordings directory
- `q` exits, stopping and saving any active recording first

## Behavior Notes

- MIDI Capture records channel voice messages, running-status channel messages,
  and SysEx into Standard MIDI Files.
- It ignores MIDI clock, active sensing, and unsupported or incomplete system
  messages.
- In TUI mode, source `0` is monitored continuously for live `RX` logging,
  recording uses source `0`, and playback uses destination `0`.

## Version

Print the application version:

```sh
./midi --version
```

Release versioning, git tag, and GitHub Release rules are documented in
[docs/versioning.md](docs/versioning.md).

## For Contributors

Run the regression tests:

```sh
make test
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup, pull request
expectations, and contribution licensing.

## License

Copyright 2026 Peter Nicholls. Licensed under the
[Apache License 2.0](LICENSE).
