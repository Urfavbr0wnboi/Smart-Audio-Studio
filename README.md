# Smart Audio Studio

A user-friendly music production app (a DAW) for beginners. It keeps the parts of a professional studio that matter and hides the complexity: everything happens in one window, audio routing is automatic, and the dark interface keeps the focus on your waveforms.

Written in C++20 with the [JUCE](https://juce.com) framework, as set out in the project report.

## What works today

### Phase 1: arranging and mixing
- **Single-window layout**: transport bar, file browser, timeline and mixer, all docked. The browser and mixer can be hidden.
- **Automatic audio output**: the system's default sound card opens on launch. *Audio Settings* lets you choose another one.
- **Import audio** (WAV, AIFF, FLAC, Ogg Vorbis, MP3). You can drag files from the browser or from your desktop onto a track, double-click a file in the browser, or use *Import Audio*. Files dropped below the last track get a new track.
- **Timeline**: waveform display, a clickable ruler, and a playhead that follows playback. Clips can be dragged along a track or onto another track, and Delete removes the selected clip. You can zoom and scroll.
- **Mixer**: each track has a volume fader, a pan knob, mute, solo and a stereo peak meter, and there is a master strip.
- **Real-time engine**: the audio thread never locks, allocates or frees memory, as described in the report's section 8.

### Phase 2: recording and projects
- **One-click recording**: press the red button or **R**. There is no "arm" step. The take goes onto the selected track, or a new track if none is selected. It starts at the playhead, and playback starts with it so you can hear the other tracks. Press R, Space or Stop to finish.
- **Automatic input selection**: the first time you record, the system's default microphone opens by itself. The meter next to the record button shows whether it is picking up sound.
- **Latency compensation**: each take is shifted by the sound card's reported input and output latency, so it lines up with what you heard.
- **Recordings are saved as 24-bit WAV files**: in `<project name> Recordings/` next to a saved project, or in `Documents/Smart Audio Studio/Recordings/` before the first save. A background thread writes them, so a slow disk can't cause glitches.
- **Projects** (`.sasproj`): New, Open, Save and Save As, from the *File* menu or with Ctrl/Cmd + N, O, S and Shift+S. The window title shows `*` when there are unsaved changes, and you are asked before anything would discard them.
- **Auto-save every 5 minutes** while there are unsaved changes. Auto-saves go to `Documents/Smart Audio Studio/Autosave/`, never over your own file, and are deleted when the app closes normally. If the app crashes, it offers to recover your work the next time it starts.

### Mapping to the report's requirements

| Requirement | Status |
| --- | --- |
| FR-01 Record from hardware inputs | Done |
| FR-02 Waveform display | Done |
| FR-03 Non-destructive editing (split, move, delete, fade) | Move and delete done; split and fade next |
| FR-04 Mixer with volume and pan | Done |
| FR-05 Native effects (EQ, compressor, reverb, delay, limiter) | Phase 3 |
| FR-06 Export to WAV / MP3 | Phase 4 |
| NFR Single window, dark high-contrast theme | Done |
| NFR Auto-save every 5 minutes | Done |

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| Space | Play / pause (stops recording if recording) |
| R | Start / stop recording |
| Home or Enter | Return to start |
| Delete / Backspace | Delete the selected clip |
| `+` / `-` | Zoom in / out |
| Ctrl or Cmd + mouse wheel | Zoom around the mouse |
| Shift + mouse wheel | Scroll sideways |
| Ctrl/Cmd + N, O, S | New, open, save project |
| Ctrl/Cmd + Shift + S | Save as |

## Project files

A `.sasproj` file is readable JSON that lists the tracks, mixer settings and clips. Audio is not copied into it: each clip points at its audio file.

- Audio inside the project's folder is stored with a relative path, so you can move or share the whole folder.
- Audio anywhere else is stored with its full path.
- If an audio file can't be found when opening, the rest of the project still opens and you are told which file is missing.

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

You can also pass a project or audio files on the command line. A project is opened, and each audio file is placed on its own track.

## Project layout

```
Source/
  Main.cpp                 Application and main window
  MainComponent.*          Window layout, keyboard shortcuts, recording controls
  ProjectDocument.*        New/open/save, unsaved-changes prompts, auto-save and recovery
  Engine/                  Audio code that has no GUI dependencies
    MixEngine.*            Real-time mixer and transport (runs on the audio thread)
    Project.*              The document: tracks and clips (message thread)
    ProjectFile.*          Reading and writing .sasproj files
    Recorder.*             Captures input to WAV through a lock-free FIFO
    AudioEngine.*          Connects the mixer and recorder to the sound card
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
5. While recording, the audio thread copies microphone input into a lock-free FIFO, and a background thread writes it to disk.

Because the engine doesn't depend on an audio device, the same code can later render the export file.

## Roadmap

- **Phase 3**: split, trim and fade clips; native EQ, compressor, reverb, delay and limiter; the one-knob "Ducking" macro; track grouping; input monitoring with feedback protection.
- **Phase 4**: export to WAV/MP3, undo/redo, first-launch guided tips, copying audio into the project folder, performance and UAT testing.
