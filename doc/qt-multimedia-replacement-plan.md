<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Plan: Replace Qt Multimedia for Talk call ringtone playback

## Current state

- The only in-repo usage of Qt Multimedia is in `src/gui/tray/CallNotificationDialog.qml`:
  - `import QtMultimedia`
  - `SoundEffect { source: "qrc:///client/theme/call-notification.wav"; loops: 9 }`
- The dialog is created from `Systray::createCallDialog(...)` in `src/gui/systray.cpp`.
- This means a single ringtone use case currently pulls in the whole Qt Multimedia stack.

## Goal

Replace Qt Multimedia with a lightweight native, cross-platform C++ API that can:

1. play an uncompressed WAV file asynchronously,
2. repeat playback for a bounded number of loops,
3. stop playback immediately when the call notification is dismissed.

## Non-goals

- No changes to notification business logic or Talk server communication.
- No new ringtone UX features.
- No migration of unrelated audio/video use cases.

## Proposed architecture

Create a small abstraction in `src/gui`:

- `RingtonePlayer` (Qt/C++ facade exposed to QML)
  - `Q_INVOKABLE void play(const QString &path, int loopCount);`
  - `Q_INVOKABLE void stop();`
  - optional `bool isPlaying() const`.
- `INativeSoundBackend` interface (internal only).
- Platform-specific backend implementations selected at compile time:
  - **Windows**: `PlaySoundW` (`winmm`) with async/loop flags.
  - **macOS**: Objective-C++ backend using native audio API (e.g. `NSSound` or `AVAudioPlayer`) with explicit retain/release handling.
  - **Linux**: native backend selected after short spike (prefer desktop-native API with minimal footprint; define fallback behavior when backend unavailable).

QML then calls the facade instead of `SoundEffect`.

## Migration steps

1. **Add API skeleton and QML bridge**
   - Introduce `RingtonePlayer` class and register it for QML use.
   - Keep behavior equivalent to current play/stop lifecycle in `CallNotificationDialog.qml`.

2. **Implement platform backends**
   - Add one backend per platform behind a shared interface.
   - Ensure `stop()` is idempotent and safe during dialog teardown.

3. **Switch the call notification dialog**
   - Remove `import QtMultimedia`.
   - Replace `SoundEffect` object with calls to `RingtonePlayer`.

4. **Build and packaging cleanup**
   - Remove Qt Multimedia runtime/build dependency where still referenced.
   - Verify installer/bundling scripts no longer include Qt Multimedia artifacts.

5. **Validation**
   - Unit tests for `RingtonePlayer` control flow (start/stop/loop argument handling) with mock backend.
   - Manual platform checks:
     - incoming call starts ringtone,
     - decline/close stops ringtone immediately,
     - ringtone stops after configured loop count,
     - no regressions in notification window lifecycle.

## Rollout and risk handling

- Land behind incremental commits (API scaffolding → backend implementations → QML switch → dependency cleanup).
- Keep a temporary compile-time fallback path during transition if needed.
- Main risks:
  - Linux backend fragmentation (PulseAudio/PipeWire/ALSA differences),
  - correct loop semantics parity across APIs,
  - teardown race conditions on dialog close.
