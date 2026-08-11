/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QMLSCREENSHOTCONTROLLER_H
#define QMLSCREENSHOTCONTROLLER_H

#include "screenshotactivitymodel.h"
#include "screenshotsystray.h"
#include "screenshotsyncstatussummary.h"
#include "screenshotusermodel.h"
#include "uiscreenshotoutput.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <memory>

class QQmlApplicationEngine;
class QQuickWindow;

namespace OCC {
class ScreenshotWizardController;

/** @brief Captures the six production QML windows and exports production status SVGs. */
class QmlScreenshotController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a controller for the validated staging directory.
     * @param outputDirectory App-container screenshot staging directory.
     * @param parent Object that owns the controller.
     */
    explicit QmlScreenshotController(const QString &outputDirectory, QObject *parent = nullptr);
    ~QmlScreenshotController() override;

    /** @brief Starts the bounded sequential screenshot workflow. */
    void start();

signals:
    /** @brief Reports zero after complete output or a nonzero failure result. */
    void finished(int exitCode);

private:
    Q_DISABLE_COPY_MOVE(QmlScreenshotController)

    enum class CaptureState {
        Idle,
        WaitingForExposure,
        WaitingForFrame,
        Settling,
        WaitingForDestruction,
        Finished,
    };

    void captureNext();
    void createCurrentWindow();
    void checkExposure();
    void handleFrameSwapped();
    void captureCurrentWindow();
    void destroyCurrentWindow();
    void handleWindowDestroyed();
    void finishSuccessfully();
    void startDeadline(const QString &operation);
    void fail(const QString &message);

    UiScreenshotOutput _output;
    QString _runId;

    // Singleton and model members are declared before the engine so they outlive it.
    ScreenshotUserModel _userModel;
    ScreenshotSystray _systray;
    ScreenshotActivityModel _activityModel;
    ScreenshotSyncStatusSummary _syncStatusModel;
    std::unique_ptr<QQmlApplicationEngine> _engine;

    QPointer<QQuickWindow> _window;
    QPointer<ScreenshotWizardController> _wizardController;
    QTimer _deadlineTimer;
    QString _deadlineOperation;
    QStringList _qmlWarnings;
    int _jobIndex = 0;
    CaptureState _state = CaptureState::Idle;
    bool _finished = false;
};

}

#endif // QMLSCREENSHOTCONTROLLER_H
