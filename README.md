# Smart Audio Studio

A user-friendly music production app (a DAW) for beginners. It keeps the parts of a professional studio that matter and hides the complexity: everything happens in one window, audio routing is automatic, and the dark interface keeps the focus on your waveforms.

Written in C++20 with the [JUCE](https://juce.com) framework, as set out in the project report.

## What works today (Phase 1)

- **Single-window layout**: transport bar, file browser, timeline and mixer, all docked. The browser and mixer can be hidden.
- **Automatic audio output**: the system's default sound card opens on launch. *Audio Settings* lets you choose another one.
- **Import audio** (WAV, AIFF, FLAC, Ogg Vorbis, MP3). You can drag files from the browser or from your desktop onto a track, double-click a file in the browser, or use *Import Audio*. Files dropped below the last track get a new track.
- **Timeline**: waveform display, a clickable ruler, and a playhead that follows playback. Clips can be dragged along a track or onto another track, and Delete removes the selected clip. You can zoom and scroll.
- **Mixer**: each track has a volume fader, a pan knob, mute, solo and a stereo peak meter, and there is a master strip.
- **Real-time engine**: the audio thread never locks, allocates or frees memory, as described in the report's section 8.

### Mapping to the report's requirements

| Requirement | Status |
| --- | --- |
| FR-01 Record from hardware inputs | Next (Phase 2) |
| FR-02 Waveform display | Done |
| FR-03 Non-destructive editing (split, move, delete, fade) | Move and delete done; split and fade next |
| FR-04 Mixer with volume and pan | Done |
| FR-05 Native effects (EQ, compressor, reverb, delay, limiter) | Phase 3 |
| FR-06 Export to WAV / MP3 | Phase 4 |
| NFR Single window, dark high-contrast theme | Done |

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| Space | Play / pause |
| Home or Enter | Return to start |
| Delete / Backspace | Delete the selected clip |
| `+` / `-` | Zoom in / out |
| Ctrl or Cmd + mouse wheel | Zoom around the mouse |
| Shift + mouse wheel | Scroll sideways |

## Building

You need CMake 3.22 or newer and a C++20 compiler. JUCE 9.0.4 is downloaded automatically when you configure.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure   # run the unit tests
```

The app is placed in `build/SmartAudioStudio_artefacts/Release/`.

- **Windows**: Visual Studio 2022 with the "Desktop development with C++" workload. Running the commands above from a Developer PowerShell works, or you can open the folder in Visual Studio.
- **macOS**: Xcode command line tools (`xcode-select --install`). Add `-G Xcode` to the configure command to get an Xcode project.
- **Linux (Debian/Ubuntu)**: install the JUCE dependencies first:
  ```bash
  sudo apt install build-essential cmake libasound2-dev libfreetype-dev libfontconfig1-dev \
      libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
      libxrandr-dev libxrender-dev libxi-dev libegl-dev libglu1-mesa-dev mesa-common-dev
  ```

If you already have JUCE checked out, add `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` to skip the download.

You can also pass audio files on the command line. Each one is placed on its own track.

## Project layout

```
Source/
  Main.cpp                 Application and main window
  MainComponent.*          Window layout and keyboard shortcuts
  Engine/                  Audio code that has no GUI dependencies
    MixEngine.*            Real-time mixer and transport (runs on the audio thread)
    Project.*              The document: tracks and clips (message thread)
    AudioEngine.*          Connects the mixer to the sound card
    AudioFileLoader.*      Decoding and sample-rate conversion
    Waveform.*             Min/max summaries for drawing waveforms
    ChannelParams.h        Lock-free volume/pan/mute/solo/meter values
  UI/                      Timeline, mixer, browser, transport bar, theme
Tests/                     Unit tests for the engine (no sound card needed)
```

## How the engine stays glitch-free

The report's MVC adaptation maps onto the code like this:

- **Model**: `Project` holds the tracks and clips and is only ever edited on the message (UI) thread.
- **View**: the components in `UI/`.
- **Audio engine**: `MixEngine` renders audio on the real-time thread.

The engine and the UI never share mutable data:

1. After every edit, `Project` builds an immutable `PlaybackGraph`, a snapshot of what to play, and hands it to the engine.
2. At the start of each audio block, the audio thread picks up the newest snapshot using a try-lock. If the UI happens to hold the lock, it simply keeps playing the previous snapshot for one more block.
3. Old snapshots are freed by a timer on the message thread, never by the audio thread.
4. Faders, pan, mute, solo and meters use `std::atomic` values, so moving a fader doesn't need a new snapshot at all.

Because the engine doesn't depend on an audio device, the same code can later render the export file.

## Roadmap

- **Phase 2**: one-click recording with automatic input selection, saving and loading projects, auto-save every 5 minutes.
- **Phase 3**: split and fade clips; native EQ, compressor, reverb, delay and limiter; the one-knob "Ducking" macro; track grouping.
- **Phase 4**: export to WAV/MP3, undo/redo, first-launch guided tips, performance and UAT testing.
