/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "notificationsoundplayer.h"
#include "notificationsoundplayer_p.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QString>

#ifdef HAVE_LIBCANBERRA
#include <canberra.h>
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcNotificationSoundPlayerLinux, "nextcloud.gui.notificationsoundplayer.linux", QtInfoMsg)

namespace {

#ifdef HAVE_LIBCANBERRA

class LinuxBackend : public NotificationSoundPlayer::Backend
{
public:
    LinuxBackend()
    {
        const int rc = ca_context_create(&_context);
        if (rc != CA_SUCCESS) {
            qCWarning(lcNotificationSoundPlayerLinux) << "ca_context_create failed:" << ca_strerror(rc);
            _context = nullptr;
        }
    }

    ~LinuxBackend() override
    {
        if (_context) {
            ca_context_cancel(_context, kEventId);
            // ca_context_destroy joins the worker thread, so no callback
            // can fire after this returns.
            ca_context_destroy(_context);
            _context = nullptr;
        }
    }

    void setSource(const QString & /*filesystemPath*/) override
    {
        // We play an XDG sound-theme event by id (`phone-incoming-call`) and
        // let libcanberra / the user's sound theme provide the actual audio.
        // The dispatcher signals this by returning `false` from
        // `needsFilesystemPath()` below, so it never resolves a filesystem
        // path in the first place. The argument is therefore ignored.
    }

    void play(int /*loops*/) override
    {
        if (!_context) {
            signalFinishedAsync();
            return;
        }

        ca_proplist *props = nullptr;
        if (ca_proplist_create(&props) != CA_SUCCESS) {
            signalFinishedAsync();
            return;
        }
        ca_proplist_sets(props, CA_PROP_EVENT_ID, "phone-incoming-call");
        ca_proplist_sets(props, CA_PROP_MEDIA_ROLE, "event");
        ca_proplist_sets(props, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");

        const int rc = ca_context_play_full(_context, kEventId, props, &finishedTrampoline, this);
        ca_proplist_destroy(props);

        if (rc != CA_SUCCESS) {
            qCWarning(lcNotificationSoundPlayerLinux)
                << "ca_context_play_full failed:" << ca_strerror(rc);
            signalFinishedAsync();
        }
    }

    void stop() override
    {
        if (_context) {
            ca_context_cancel(_context, kEventId);
        }
    }

    [[nodiscard]] bool handlesLoopsNatively() const override { return false; }
    [[nodiscard]] bool needsFilesystemPath() const override { return false; }

private:
    static void finishedTrampoline(ca_context * /*ctx*/, uint32_t /*id*/, int errorCode, void *userdata)
    {
        // Fires on the libcanberra worker thread. The userdata pointer is
        // valid because ~LinuxBackend() blocks in ca_context_destroy() until
        // any in-flight callback has returned.
        if (errorCode == CA_ERROR_CANCELED) {
            // Cancellation (via stop() or destruction) is not a natural end of
            // playback — never advance the dispatcher's loop bookkeeping for it.
            return;
        }
        auto *self = static_cast<LinuxBackend *>(userdata);
        if (self->onFinished) {
            self->onFinished();
        }
    }

    void signalFinishedAsync()
    {
        if (!onFinished) {
            return;
        }
        auto cb = onFinished;
        QMetaObject::invokeMethod(qApp, [cb]() { cb(); }, Qt::QueuedConnection);
    }

    ca_context *_context = nullptr;
    static constexpr uint32_t kEventId = 1;
};

#else // !HAVE_LIBCANBERRA

class LinuxNoOpBackend : public NotificationSoundPlayer::Backend
{
public:
    void setSource(const QString &) override {}

    void play(int) override
    {
        if (!_warnedOnce) {
            qCWarning(lcNotificationSoundPlayerLinux)
                << "libcanberra was not available at build time; notification sound is silent";
            _warnedOnce = true;
        }
        if (onFinished) {
            auto cb = onFinished;
            QMetaObject::invokeMethod(qApp, [cb]() { cb(); }, Qt::QueuedConnection);
        }
    }

    void stop() override {}

    [[nodiscard]] bool handlesLoopsNatively() const override { return true; }
    [[nodiscard]] bool needsFilesystemPath() const override { return false; }

private:
    bool _warnedOnce = false;
};

#endif // HAVE_LIBCANBERRA

}

std::unique_ptr<NotificationSoundPlayer::Backend> createNotificationSoundPlayerBackend()
{
#ifdef HAVE_LIBCANBERRA
    return std::make_unique<LinuxBackend>();
#else
    return std::make_unique<LinuxNoOpBackend>();
#endif
}

}
