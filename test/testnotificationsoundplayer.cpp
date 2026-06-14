/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>
#include <QtTest>

#include "gui/notificationsoundplayer.h"
#include "gui/notificationsoundplayer_p.h"

using namespace OCC;

namespace {

class TestBackend : public NotificationSoundPlayer::Backend
{
public:
    explicit TestBackend(bool nativeLoops, bool wantsFilesystemPath = true)
        : _nativeLoops(nativeLoops)
        , _wantsFilesystemPath(wantsFilesystemPath)
    {
    }

    void setSource(const QString &sourceArg) override { source = sourceArg; }
    void play(int loops) override
    {
        ++playCallCount;
        lastLoopsArg = loops;
    }
    void stop() override { ++stopCallCount; }
    [[nodiscard]] bool handlesLoopsNatively() const override { return _nativeLoops; }
    [[nodiscard]] bool needsFilesystemPath() const override { return _wantsFilesystemPath; }

    void fireFinished()
    {
        if (onFinished) {
            onFinished();
        }
    }

    QString source;
    int playCallCount = 0;
    int stopCallCount = 0;
    int lastLoopsArg = -1;

private:
    bool _nativeLoops;
    bool _wantsFilesystemPath;
};

}

class TestNotificationSoundPlayer : public QObject
{
    Q_OBJECT

private:
    static QString writeTempSoundFile()
    {
        auto *tmp = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/nc-sound-test-XXXXXX.wav"));
        tmp->setAutoRemove(true);
        if (!tmp->open()) {
            delete tmp;
            return {};
        }
        tmp->write("RIFF\0\0\0\0WAVE", 12);
        const auto path = tmp->fileName();
        tmp->close();
        // Leak deliberately: keep the file alive for the test's lifetime
        // by attaching it as a child of qApp.
        tmp->setParent(qApp);
        return path;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // nextcloudCore is statically linked into the test binary, so the QRC
        // collections it embeds (`resources.qrc` and the generated `theme.qrc`)
        // need to be initialised explicitly before any `qrc:` lookup runs.
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
    }

    // ----- resolveToFilesystemPath -----
    //
    // The extractor is a generic QRC-to-disk copier; it doesn't care about
    // the resource's file type. We use an arbitrary always-embedded SVG
    // (`theme/delete.svg`) so the tests run on every platform, including
    // Linux where the production WAV is intentionally not embedded.

    void resolve_qrcExtractsToCache()
    {
        const auto path = NotificationSoundPlayer::resolveToFilesystemPath(
            QStringLiteral("qrc:///client/theme/delete.svg"));
        QVERIFY(!path.isEmpty());
        QVERIFY(QFile::exists(path));
    }

    void resolve_qrcIsIdempotent()
    {
        const auto first = NotificationSoundPlayer::resolveToFilesystemPath(
            QStringLiteral("qrc:///client/theme/delete.svg"));
        const auto second = NotificationSoundPlayer::resolveToFilesystemPath(
            QStringLiteral("qrc:///client/theme/delete.svg"));
        QVERIFY(!first.isEmpty());
        QCOMPARE(first, second);
    }

    void resolve_fileUrlReturnsLocalPath()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const auto fileUrl = QUrl::fromLocalFile(tmp.fileName()).toString();
        const auto resolved = NotificationSoundPlayer::resolveToFilesystemPath(fileUrl);
        QCOMPARE(resolved, tmp.fileName());
    }

    void resolve_plainPathReturnsAsIs()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const auto resolved = NotificationSoundPlayer::resolveToFilesystemPath(tmp.fileName());
        QCOMPARE(resolved, tmp.fileName());
    }

    void resolve_emptyReturnsEmpty()
    {
        QCOMPARE(NotificationSoundPlayer::resolveToFilesystemPath({}), QString());
    }

    void resolve_nonexistentPathReturnsEmpty()
    {
        QCOMPARE(NotificationSoundPlayer::resolveToFilesystemPath(QStringLiteral("/definitely/does/not/exist.wav")),
                 QString());
    }

    // ----- Loop bookkeeping -----

    void loops_perPlayBackendReissuesPlayUntilExhausted()
    {
        const auto soundPath = writeTempSoundFile();
        QVERIFY(!soundPath.isEmpty());

        auto backendOwned = std::make_unique<TestBackend>(/*nativeLoops*/ false);
        auto *backend = backendOwned.get();
        NotificationSoundPlayer player(std::move(backendOwned), this);
        player.setSource(soundPath);
        player.setLoops(3);

        player.play();
        QCOMPARE(backend->playCallCount, 1);
        QVERIFY(player.isPlaying());

        backend->fireFinished();
        QCoreApplication::processEvents();
        QCOMPARE(backend->playCallCount, 2);
        QVERIFY(player.isPlaying());

        backend->fireFinished();
        QCoreApplication::processEvents();
        QCOMPARE(backend->playCallCount, 3);
        QVERIFY(player.isPlaying());

        backend->fireFinished();
        QCoreApplication::processEvents();
        QCOMPARE(backend->playCallCount, 3);
        QVERIFY(!player.isPlaying());
    }

    void loops_nativeBackendDoesNotReissuePlay()
    {
        const auto soundPath = writeTempSoundFile();
        QVERIFY(!soundPath.isEmpty());

        auto backendOwned = std::make_unique<TestBackend>(/*nativeLoops*/ true);
        auto *backend = backendOwned.get();
        NotificationSoundPlayer player(std::move(backendOwned), this);
        player.setSource(soundPath);
        player.setLoops(9);

        player.play();
        QCOMPARE(backend->playCallCount, 1);
        QCOMPARE(backend->lastLoopsArg, 9);
        QVERIFY(player.isPlaying());

        backend->fireFinished();
        QCoreApplication::processEvents();
        QCOMPARE(backend->playCallCount, 1);
        QVERIFY(!player.isPlaying());
    }

    void loops_stopMidSequenceCancelsRemainingPlays()
    {
        const auto soundPath = writeTempSoundFile();
        QVERIFY(!soundPath.isEmpty());

        auto backendOwned = std::make_unique<TestBackend>(/*nativeLoops*/ false);
        auto *backend = backendOwned.get();
        NotificationSoundPlayer player(std::move(backendOwned), this);
        player.setSource(soundPath);
        player.setLoops(5);

        player.play();
        backend->fireFinished();
        QCoreApplication::processEvents();
        QCOMPARE(backend->playCallCount, 2);

        player.stop();
        QCOMPARE(backend->stopCallCount, 1);
        QVERIFY(!player.isPlaying());

        // A late finished callback that races the stop must not re-issue play.
        backend->fireFinished();
        QCoreApplication::processEvents();
        QCOMPARE(backend->playCallCount, 2);
        QVERIFY(!player.isPlaying());
    }

    void play_emptySourceIsNoop()
    {
        auto backendOwned = std::make_unique<TestBackend>(true);
        auto *backend = backendOwned.get();
        NotificationSoundPlayer player(std::move(backendOwned), this);
        player.setLoops(3);

        player.play();
        QCOMPARE(backend->playCallCount, 0);
        QVERIFY(!player.isPlaying());
    }

    void play_backendThatDoesNotNeedFilesystemPath_skipsResolution()
    {
        // Source is a bogus QRC URL that doesn't exist; if the dispatcher
        // tried to extract it, play() would bail with an empty resolved path.
        // Because the backend declares it doesn't need a filesystem path,
        // the dispatcher should hand the raw source over and play anyway.
        auto backendOwned = std::make_unique<TestBackend>(/*nativeLoops*/ true, /*wantsFilesystemPath*/ false);
        auto *backend = backendOwned.get();
        NotificationSoundPlayer player(std::move(backendOwned), this);
        player.setSource(QStringLiteral("qrc:///does/not/exist.wav"));
        player.setLoops(3);

        player.play();
        QCOMPARE(backend->playCallCount, 1);
        QCOMPARE(backend->source, QStringLiteral("qrc:///does/not/exist.wav"));
        QVERIFY(player.isPlaying());
    }

    void play_zeroLoopsIsNoop()
    {
        const auto soundPath = writeTempSoundFile();
        QVERIFY(!soundPath.isEmpty());

        auto backendOwned = std::make_unique<TestBackend>(true);
        auto *backend = backendOwned.get();
        NotificationSoundPlayer player(std::move(backendOwned), this);
        player.setSource(soundPath);
        player.setLoops(0);

        player.play();
        QCOMPARE(backend->playCallCount, 0);
        QVERIFY(!player.isPlaying());
    }
};

QTEST_MAIN(TestNotificationSoundPlayer)
#include "testnotificationsoundplayer.moc"
