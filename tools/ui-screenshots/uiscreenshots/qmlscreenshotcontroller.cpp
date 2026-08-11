/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qmlscreenshotcontroller.h"

#include "qmlscreenshotcaptureutils.h"
#include "qmlscreenshotregistrationutils.h"
#include "screenshotwizardcontroller.h"
#include "uiscreenshotmanifest.h"
#include "uiscreenshotmode.h"
#include "wizard/accountwizardcontroller.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickWindow>
#include <QVariantMap>

#include <cstdlib>

namespace OCC {

namespace {
constexpr auto operationTimeoutMilliseconds = 10000;
constexpr auto imageSettleMilliseconds = 200;
constexpr auto imageStatusLoading = 2;
constexpr auto imageStatusError = 3;

QString qmlErrors(const QList<QQmlError> &errors)
{
    auto messages = QStringList{};
    messages.reserve(errors.size());
    for (const auto &error : errors) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1String(" | "));
}

enum class ImageReadiness {
    Ready,
    Loading,
    Error,
};

struct ImageReadinessResult
{
    ImageReadiness readiness = ImageReadiness::Ready;
    QString error;
};

ImageReadinessResult imageReadiness(QQuickWindow &window)
{
    auto result = ImageReadinessResult{};
    const auto objects = window.findChildren<QObject *>();
    for (const auto *object : objects) {
        if (!object->inherits("QQuickImageBase")
            && !object->inherits("QQuickImage")
            && !object->inherits("QQuickBorderImage")
            && !object->inherits("QQuickAnimatedImage")
            && !object->inherits("QQuickIconImage")) {
            continue;
        }

        const auto status = object->property("status");
        if (!status.isValid()) {
            continue;
        }
        if (status.toInt() == imageStatusError) {
            const auto objectDescription = object->objectName().isEmpty()
                ? QString::fromLatin1(object->metaObject()->className())
                : object->objectName();
            return {
                ImageReadiness::Error,
                QStringLiteral("QML image failed to load: %1.").arg(objectDescription),
            };
        }
        if (status.toInt() == imageStatusLoading) {
            result.readiness = ImageReadiness::Loading;
        }
    }
    return result;
}
}

QmlScreenshotController::QmlScreenshotController(const QString &outputDirectory, QObject *parent)
    : QObject(parent)
    , _output(outputDirectory)
    , _userModel()
    , _systray()
    , _activityModel()
    , _syncStatusModel()
{
    _deadlineTimer.setSingleShot(true);
    connect(&_deadlineTimer, &QTimer::timeout, this, [this] {
        const auto &jobs = UiScreenshots::qmlScreenshotJobs();
        fail(QStringLiteral("Timed out while %1 for %2.")
                 .arg(_deadlineOperation, _jobIndex < jobs.size() ? jobs.at(_jobIndex).outputName : QStringLiteral("phase completion")));
    });
}

QmlScreenshotController::~QmlScreenshotController()
{
    if (_window) {
        disconnect(_window, nullptr, this, nullptr);
        delete _window.data();
        _window = nullptr;
    }
    _engine.reset();
}

void QmlScreenshotController::start()
{
    if (_state != CaptureState::Idle || _finished) {
        fail(QStringLiteral("QML screenshot controller was started more than once."));
        return;
    }

    auto error = QString{};
    if (!_output.beginQmlRun(&_runId, &error)) {
        fail(error);
        return;
    }

    qCInfo(UiScreenshots::lcUiScreenshots) << "Parsed screenshot phase qml";
    qCInfo(UiScreenshots::lcUiScreenshots) << "Validated screenshot staging path" << _output.directory();
    UiScreenshots::registerQmlTypes(_userModel, _systray);
    _engine = std::make_unique<QQmlApplicationEngine>();
    connect(_engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) {
            _qmlWarnings.append(warning.toString());
        }
    });
    UiScreenshots::configureQmlEngine(*_engine);
    captureNext();
}

void QmlScreenshotController::captureNext()
{
    if (_finished) {
        return;
    }
    if (_jobIndex >= UiScreenshots::qmlScreenshotJobs().size()) {
        finishSuccessfully();
        return;
    }
    createCurrentWindow();
}

void QmlScreenshotController::createCurrentWindow()
{
    _qmlWarnings.clear();
    const auto &job = UiScreenshots::qmlScreenshotJobs().at(_jobIndex);
    qCInfo(UiScreenshots::lcUiScreenshots) << "Opening production QML component" << job.componentUrl << "for" << job.outputName;

    auto initialProperties = QVariantMap{};
    switch (job.kind) {
    case UiScreenshots::QmlScreenshotJobKind::Activities:
        initialProperties.insert(QStringLiteral("account"), QVariantMap{
            {QStringLiteral("avatar"), _userModel.currentUser()->avatar()},
            {QStringLiteral("name"), _userModel.currentUser()->name()},
            {QStringLiteral("server"), _userModel.currentUser()->server()},
            {QStringLiteral("accentColor"), _userModel.currentUser()->accentColor()},
        });
        initialProperties.insert(QStringLiteral("activityUser"), QVariant::fromValue<QObject *>(_userModel.currentUser()));
        initialProperties.insert(QStringLiteral("activityModel"), QVariant::fromValue<QObject *>(&_activityModel));
        initialProperties.insert(QStringLiteral("syncStatusModel"), QVariant::fromValue<QObject *>(&_syncStatusModel));
        break;
    case UiScreenshots::QmlScreenshotJobKind::UserStatus:
        initialProperties.insert(QStringLiteral("userIndex"), 0);
        break;
    case UiScreenshots::QmlScreenshotJobKind::Assistant:
        initialProperties.insert(QStringLiteral("userIndex"), 0);
        initialProperties.insert(QStringLiteral("currentUser"), QVariant::fromValue<QObject *>(_userModel.currentUser()));
        break;
    case UiScreenshots::QmlScreenshotJobKind::WizardServer:
    case UiScreenshots::QmlScreenshotJobKind::WizardBrowserAuth:
    case UiScreenshots::QmlScreenshotJobKind::WizardSyncOptions: {
        _wizardController = new ScreenshotWizardController(this);
        if (job.kind == UiScreenshots::QmlScreenshotJobKind::WizardBrowserAuth) {
            _wizardController->setCurrentStepForScreenshot(AccountWizardController::BrowserAuthStep);
        } else if (job.kind == UiScreenshots::QmlScreenshotJobKind::WizardSyncOptions) {
            _wizardController->setCurrentStepForScreenshot(AccountWizardController::SyncOptionsStep);
            _wizardController->setSyncModeForScreenshot(AccountWizardController::VirtualFiles);
        } else {
            _wizardController->setCurrentStepForScreenshot(AccountWizardController::ServerStep);
        }
        initialProperties.insert(QStringLiteral("controller"), QVariant::fromValue<QObject *>(_wizardController));
        break;
    }
    }

    auto component = QQmlComponent(_engine.get(), job.componentUrl, QQmlComponent::PreferSynchronous);
    if (component.isError() || !component.isReady()) {
        fail(QStringLiteral("Could not load %1: %2").arg(job.componentUrl.toString(), qmlErrors(component.errors())));
        return;
    }

    auto *createdObject = component.createWithInitialProperties(initialProperties);
    if (component.isError()) {
        if (createdObject) {
            createdObject->deleteLater();
        }
        fail(QStringLiteral("Could not create %1: %2").arg(job.componentUrl.toString(), qmlErrors(component.errors())));
        return;
    }
    _window = qobject_cast<QQuickWindow *>(createdObject);
    if (!_window) {
        if (createdObject) {
            createdObject->deleteLater();
        }
        fail(QStringLiteral("Production component did not create a QQuickWindow: %1").arg(job.componentUrl.toString()));
        return;
    }

    const auto wizardWindow = job.kind == UiScreenshots::QmlScreenshotJobKind::WizardServer
        || job.kind == UiScreenshots::QmlScreenshotJobKind::WizardBrowserAuth
        || job.kind == UiScreenshots::QmlScreenshotJobKind::WizardSyncOptions;
    UiScreenshots::configureQuickWindow(*_window, wizardWindow);
    connect(_window, &QQuickWindow::frameSwapped, this, &QmlScreenshotController::handleFrameSwapped);
    connect(_window, &QObject::destroyed, this, &QmlScreenshotController::handleWindowDestroyed);

    _state = CaptureState::WaitingForExposure;
    startDeadline(QStringLiteral("waiting for window exposure"));
    _window->show();
    _window->raise();
    _window->requestActivate();
    QTimer::singleShot(0, this, &QmlScreenshotController::checkExposure);
}

void QmlScreenshotController::checkExposure()
{
    if (_finished || _state != CaptureState::WaitingForExposure) {
        return;
    }
    if (!_window) {
        fail(QStringLiteral("QML window was destroyed before exposure."));
        return;
    }
    if (!_window->isExposed()) {
        QTimer::singleShot(25, this, &QmlScreenshotController::checkExposure);
        return;
    }

    const auto &job = UiScreenshots::qmlScreenshotJobs().at(_jobIndex);
    const auto wizardWindow = job.kind == UiScreenshots::QmlScreenshotJobKind::WizardServer
        || job.kind == UiScreenshots::QmlScreenshotJobKind::WizardBrowserAuth
        || job.kind == UiScreenshots::QmlScreenshotJobKind::WizardSyncOptions;
    if (wizardWindow) {
        UiScreenshots::styleWizardTitleBar(*_window);
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Window exposed for" << job.outputName;
    _state = CaptureState::WaitingForFrame;
    startDeadline(QStringLiteral("waiting for a post-exposure frame"));
    _window->requestUpdate();
}

void QmlScreenshotController::handleFrameSwapped()
{
    if (_finished || _state != CaptureState::WaitingForFrame || !_window) {
        return;
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Post-exposure frame rendered for" << UiScreenshots::qmlScreenshotJobs().at(_jobIndex).outputName;
    _state = CaptureState::Settling;
    startDeadline(QStringLiteral("settling QML layout and images"));
    _window->requestUpdate();
    QTimer::singleShot(0, this, [this] {
        if (_state == CaptureState::Settling && _window) {
            QTimer::singleShot(imageSettleMilliseconds, this, &QmlScreenshotController::captureCurrentWindow);
        }
    });
}

void QmlScreenshotController::captureCurrentWindow()
{
    if (_finished || _state != CaptureState::Settling || !_window) {
        return;
    }
    const auto &job = UiScreenshots::qmlScreenshotJobs().at(_jobIndex);
    if (!_qmlWarnings.isEmpty()) {
        fail(QStringLiteral("Runtime QML warning while rendering %1: %2")
                 .arg(job.outputName, _qmlWarnings.join(QLatin1String(" | "))));
        return;
    }

    const auto readiness = imageReadiness(*_window);
    if (readiness.readiness == ImageReadiness::Error) {
        fail(readiness.error);
        return;
    }
    if (readiness.readiness == ImageReadiness::Loading) {
        QTimer::singleShot(25, this, &QmlScreenshotController::captureCurrentWindow);
        return;
    }

    const auto image = _window->grabWindow();
    auto error = QString{};
    if (!_output.writePng(job.outputName, image, &error)) {
        fail(error);
        return;
    }
    destroyCurrentWindow();
}

void QmlScreenshotController::destroyCurrentWindow()
{
    if (!_window) {
        fail(QStringLiteral("QML window disappeared before teardown."));
        return;
    }
    _state = CaptureState::WaitingForDestruction;
    startDeadline(QStringLiteral("waiting for QML window destruction"));
    _window->close();
    _window->deleteLater();
}

void QmlScreenshotController::handleWindowDestroyed()
{
    _window = nullptr;
    if (_finished) {
        return;
    }
    if (_state != CaptureState::WaitingForDestruction) {
        fail(QStringLiteral("QML window was destroyed before its screenshot completed."));
        return;
    }
    _deadlineTimer.stop();
    if (_wizardController) {
        delete _wizardController.data();
        _wizardController = nullptr;
    }
    ++_jobIndex;
    _state = CaptureState::Idle;
    QTimer::singleShot(0, this, &QmlScreenshotController::captureNext);
}

void QmlScreenshotController::finishSuccessfully()
{
    auto error = QString{};
    if (!_output.exportStatusSvgs(&error) || !_output.completeQmlRun(_runId, &error)) {
        fail(error);
        return;
    }
    _state = CaptureState::Finished;
    _finished = true;
    qCInfo(UiScreenshots::lcUiScreenshots) << "QML screenshot phase completed successfully";
    emit finished(EXIT_SUCCESS);
}

void QmlScreenshotController::startDeadline(const QString &operation)
{
    _deadlineOperation = operation;
    _deadlineTimer.start(operationTimeoutMilliseconds);
}

void QmlScreenshotController::fail(const QString &message)
{
    if (_finished) {
        return;
    }
    _finished = true;
    _state = CaptureState::Finished;
    _deadlineTimer.stop();
    qCCritical(UiScreenshots::lcUiScreenshots) << message;
    if (_window) {
        _window->close();
        _window->deleteLater();
    }
    qCCritical(UiScreenshots::lcUiScreenshots) << "QML screenshot phase failed";
    emit finished(EXIT_FAILURE);
}

}
