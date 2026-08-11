/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NATIVESCREENSHOTCONTROLLER_H
#define NATIVESCREENSHOTCONTROLLER_H

#include "uiscreenshotmanifest.h"
#include "uiscreenshotoutput.h"

#include <QObject>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>

#include <memory>

class FolderManTestHelper;
class QWidget;

namespace OCC {
class AccountState;
class SettingsDialog;

/** @brief Captures six production QWidget surfaces with an isolated disposable fixture. */
class NativeScreenshotController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a native controller after staging and profile isolation were validated.
     * @param output Prepared output boundary copied from the pre-application check.
     * @param runId UUID shared with the completed QML process.
     * @param parent Object that owns the controller.
     */
    explicit NativeScreenshotController(const UiScreenshotOutput &output,
        QString runId,
        QObject *parent = nullptr);
    ~NativeScreenshotController() override;

    /** @brief Starts the bounded sequential native-dialog workflow. */
    void start();

signals:
    /** @brief Reports zero after complete output or a nonzero failure result. */
    void finished(int exitCode);

private:
    Q_DISABLE_COPY_MOVE(NativeScreenshotController)

    using Job = UiScreenshots::NativeScreenshotJobKind;

    enum class State {
        Idle,
        WaitingForExposure,
        Settling,
        WaitingForDestruction,
        Finished,
    };

    void openCurrentDialog();
    void waitForCurrentWidget(QWidget *widget);
    void checkWidgetExposure();
    void settleCurrentWidget();
    void handleWidgetSettled();
    void captureCurrentWidget();
    void closeCurrentWidget(Job nextJob);
    void handleWidgetDestroyed();
    void closeSettingsDialog();
    void finishSuccessfully();
    void startDeadline(const QString &operation);
    void fail(const QString &message);
    [[nodiscard]] QString currentOutputName() const;

    UiScreenshotOutput _output;
    const QString _runId;
    QPointer<QWidget> _widget;
    QPointer<SettingsDialog> _settingsDialog;
    QPointer<AccountState> _screenshotAccountState;
    QMetaObject::Connection _widgetDestroyedConnection;
    std::unique_ptr<FolderManTestHelper> _folderManHelper;
    QTimer _deadlineTimer;
    QString _deadlineOperation;
    Job _job = Job::User;
    Job _nextJob = Job::Finished;
    State _state = State::Idle;
    bool _settingsPageSelected = false;
    bool _finished = false;
};

}

#endif // NATIVESCREENSHOTCONTROLLER_H
