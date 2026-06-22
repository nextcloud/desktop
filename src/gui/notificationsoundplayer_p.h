/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "notificationsoundplayer.h"

#include <functional>
#include <memory>

namespace OCC {

class NotificationSoundPlayer::Backend
{
public:
    virtual ~Backend() = default;

    virtual void setSource(const QString &filesystemPath) = 0;

    // For natively-looping backends (macOS, Windows): play the entire
    // sequence of `loops` plays in one shot; invoke onFinished once at the
    // end. For per-play backends (Linux libcanberra): play once; invoke
    // onFinished after that single play (`loops` is ignored). The
    // dispatcher re-issues play() until the requested loop count is met.
    virtual void play(int loops) = 0;

    virtual void stop() = 0;

    // Return true if play(loops) plays the whole sequence in one shot and
    // invokes onFinished only when the sequence is over. Return false if
    // play() plays exactly once and the dispatcher must re-issue play()
    // to drive the loop forward.
    [[nodiscard]] virtual bool handlesLoopsNatively() const = 0;

    // Return true if the backend expects setSource() to receive a real
    // filesystem path. The dispatcher will then resolve qrc:/file:/plain
    // sources through resolveToFilesystemPath() before handing them over.
    // Return false if the backend ignores the source (e.g. libcanberra
    // playing an XDG sound-theme event by id) — the dispatcher will then
    // skip the extraction step entirely.
    [[nodiscard]] virtual bool needsFilesystemPath() const = 0;

    // Set by the dispatcher after construction. Backends must guarantee
    // that no invocation can race with their destructor — destruction must
    // join/cancel any worker threads first.
    std::function<void()> onFinished;
};

// Defined per-platform in notificationsoundplayer_{mac,win,linux}.cpp/.mm
// (or in notificationsoundplayer.cpp as a no-op fallback when no native
// backend is selected by the build).
std::unique_ptr<NotificationSoundPlayer::Backend> createNotificationSoundPlayerBackend();

}
