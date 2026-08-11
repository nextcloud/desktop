/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nativescreenshotcontroller.h"

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "creds/dummycredentials.h"
#include "folderman.h"
#include "foldermantestutils.h"
#ifdef Q_OS_MACOS
#include "foregroundbackground_interface.h"
#endif
#include "ignorelisteditor.h"
#include "nativescreenshotcaptureutils.h"
#include "settingsdialog.h"
#include "uiscreenshotmode.h"

#include <QMetaObject>
#include <QUrl>
#include <QWidget>
#include <QWindow>

#include <cstdlib>
#include <utility>

namespace OCC {

namespace {
constexpr auto operationTimeoutMilliseconds = 10000;
constexpr auto widgetSettleMilliseconds = 250;
}

NativeScreenshotController::NativeScreenshotController(const UiScreenshotOutput &output,
    QString runId,
    QObject *parent)
    : QObject(parent)
    , _output(output)
    , _runId(std::move(runId))
{
    _deadlineTimer.setSingleShot(true);
    connect(&_deadlineTimer, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("Timed out while %1 for %2.").arg(_deadlineOperation, currentOutputName()));
    });
}

NativeScreenshotController::~NativeScreenshotController()
{
    _deadlineTimer.stop();
    const auto settingsDialog = _settingsDialog.data();
    if (_widget && _widget != settingsDialog) {
        delete _widget.data();
    }
    if (settingsDialog) {
        delete settingsDialog;
    }
}

void NativeScreenshotController::start()
{
    if (_state != State::Idle || _finished) {
        fail(QStringLiteral("Native screenshot controller was started more than once."));
        return;
    }
    if (FolderMan::instance()) {
        fail(QStringLiteral("Native screenshot phase unexpectedly found an initialized folder manager."));
        return;
    }
    _folderManHelper = std::make_unique<FolderManTestHelper>();

    qCInfo(UiScreenshots::lcUiScreenshots) << "Parsed screenshot phase native";
    qCInfo(UiScreenshots::lcUiScreenshots) << "Validated screenshot staging path" << _output.directory();
    qCInfo(UiScreenshots::lcUiScreenshots) << "Initialized disposable native screenshot fixture";
    openCurrentDialog();
}

void NativeScreenshotController::openCurrentDialog()
{
    if (_finished) {
        return;
    }
    _state = State::Idle;
    _settingsPageSelected = false;

    switch (_job) {
    case Job::User: {
        if (!AccountManager::instance()->accounts().isEmpty()) {
            fail(QStringLiteral("Disposable native screenshot profile unexpectedly contains a configured account."));
            return;
        }

        qCInfo(UiScreenshots::lcUiScreenshots) << "Opening production Settings dialog";
        _settingsDialog = new SettingsDialog(nullptr);
        _settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
#ifdef Q_OS_MACOS
        _settingsDialog->installEventFilter(new ForegroundBackground(_settingsDialog));
#endif
        _settingsDialog->open();

        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.com")));
        account->setDavUser(QStringLiteral("alex"));
        account->setDavDisplayName(QStringLiteral("Alex Morgan"));
        auto *credentials = new DummyCredentials;
        credentials->_user = QStringLiteral("alex");
        account->setCredentials(credentials);

        _screenshotAccountState = new AccountState(account);
        _screenshotAccountState->setParent(this);
        _screenshotAccountState->signOutByUi();
        if (!QMetaObject::invokeMethod(_settingsDialog,
                "accountAdded",
                Qt::DirectConnection,
                Q_ARG(OCC::AccountState*, _screenshotAccountState.data()))) {
            fail(QStringLiteral("Could not add the deterministic User Settings fixture to the production Settings dialog."));
            return;
        }

        waitForCurrentWidget(_settingsDialog);
        return;
    }
    case Job::General:
    case Job::Advanced:
    case Job::Info:
        fail(QStringLiteral("Settings pages must reuse the existing Settings dialog."));
        return;
    case Job::Network: {
        qCInfo(UiScreenshots::lcUiScreenshots) << "Opening production Network Settings dialog";
        auto error = QString{};
        auto *dialog = UiScreenshots::openNetworkSettingsDialog(_settingsDialog, &error);
        if (!dialog) {
            fail(error);
            return;
        }
        waitForCurrentWidget(dialog);
        return;
    }
    case Job::IgnoredFiles: {
        qCInfo(UiScreenshots::lcUiScreenshots) << "Opening production ignored-files editor";
        auto error = QString{};
        auto *editor = UiScreenshots::openIgnoreListEditor(_settingsDialog, &error);
        if (!editor) {
            fail(error);
            return;
        }
        waitForCurrentWidget(editor);
        return;
    }
    case Job::Finished:
        closeSettingsDialog();
        return;
    }
}

void NativeScreenshotController::waitForCurrentWidget(QWidget *widget)
{
    if (!widget) {
        fail(QStringLiteral("Native screenshot job created a null widget for %1.").arg(currentOutputName()));
        return;
    }
    disconnect(_widgetDestroyedConnection);
    _widget = widget;
    _widgetDestroyedConnection = connect(_widget, &QObject::destroyed, this, &NativeScreenshotController::handleWidgetDestroyed);
    _state = State::WaitingForExposure;
    startDeadline(QStringLiteral("waiting for native window exposure"));
    QTimer::singleShot(0, this, &NativeScreenshotController::checkWidgetExposure);
}

void NativeScreenshotController::checkWidgetExposure()
{
    if (_finished || _state != State::WaitingForExposure) {
        return;
    }
    if (!_widget) {
        fail(QStringLiteral("Native widget was destroyed before exposure."));
        return;
    }
    const auto window = _widget->windowHandle();
    if (!_widget->isVisible() || !window || !window->isExposed()) {
        QTimer::singleShot(25, this, &NativeScreenshotController::checkWidgetExposure);
        return;
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Native window exposed for" << currentOutputName();
    settleCurrentWidget();
}

void NativeScreenshotController::settleCurrentWidget()
{
    if (!_widget) {
        fail(QStringLiteral("Native widget disappeared before layout settlement."));
        return;
    }
    UiScreenshots::activateWidgetLayouts(_widget);
    _state = State::Settling;
    startDeadline(QStringLiteral("settling native layout and painting"));
    QTimer::singleShot(widgetSettleMilliseconds, this, &NativeScreenshotController::handleWidgetSettled);
}

void NativeScreenshotController::handleWidgetSettled()
{
    if (_finished || _state != State::Settling || !_widget) {
        return;
    }

    if ((_job == Job::User || _job == Job::General || _job == Job::Advanced || _job == Job::Info)
        && !_settingsPageSelected) {
        auto error = QString{};
        auto page = UiScreenshots::SettingsPage::User;
        switch (_job) {
        case Job::User:
            page = UiScreenshots::SettingsPage::User;
            break;
        case Job::General:
            page = UiScreenshots::SettingsPage::General;
            break;
        case Job::Advanced:
            page = UiScreenshots::SettingsPage::Advanced;
            break;
        case Job::Info:
            page = UiScreenshots::SettingsPage::Info;
            break;
        case Job::Network:
        case Job::IgnoredFiles:
        case Job::Finished:
            Q_UNREACHABLE();
            break;
        }
        if (!UiScreenshots::selectSettingsPage(_settingsDialog, page, &error)) {
            fail(error);
            return;
        }
        _settingsPageSelected = true;
        settleCurrentWidget();
        return;
    }
    captureCurrentWidget();
}

void NativeScreenshotController::captureCurrentWidget()
{
    auto error = QString{};
    const auto outputName = currentOutputName();
    if (!UiScreenshots::captureWidget(_output, _widget, outputName, &error)) {
        fail(error);
        return;
    }

    switch (_job) {
    case Job::User:
        _job = Job::General;
        _settingsPageSelected = false;
        QTimer::singleShot(0, this, &NativeScreenshotController::handleWidgetSettled);
        return;
    case Job::General:
        _job = Job::Advanced;
        _settingsPageSelected = false;
        QTimer::singleShot(0, this, &NativeScreenshotController::handleWidgetSettled);
        return;
    case Job::Advanced:
        _job = Job::Info;
        _settingsPageSelected = false;
        QTimer::singleShot(0, this, &NativeScreenshotController::handleWidgetSettled);
        return;
    case Job::Info:
        _job = Job::Network;
        _state = State::Idle;
        QTimer::singleShot(0, this, &NativeScreenshotController::openCurrentDialog);
        return;
    case Job::Network:
        closeCurrentWidget(Job::IgnoredFiles);
        return;
    case Job::IgnoredFiles:
        closeCurrentWidget(Job::Finished);
        return;
    case Job::Finished:
        fail(QStringLiteral("Attempted to capture after native jobs completed."));
        return;
    }
}

void NativeScreenshotController::closeCurrentWidget(const Job nextJob)
{
    if (!_widget) {
        fail(QStringLiteral("Native widget disappeared before teardown."));
        return;
    }
    _nextJob = nextJob;
    _state = State::WaitingForDestruction;
    startDeadline(QStringLiteral("waiting for native window destruction"));
    _widget->close();
    _widget->deleteLater();
}

void NativeScreenshotController::handleWidgetDestroyed()
{
    _widget = nullptr;
    _widgetDestroyedConnection = QMetaObject::Connection{};
    if (_finished) {
        return;
    }
    if (_state != State::WaitingForDestruction) {
        fail(QStringLiteral("Native widget was destroyed before its screenshot completed."));
        return;
    }
    _deadlineTimer.stop();
    _job = _nextJob;
    _state = State::Idle;
    QTimer::singleShot(0, this, &NativeScreenshotController::openCurrentDialog);
}

void NativeScreenshotController::closeSettingsDialog()
{
    if (!_settingsDialog) {
        finishSuccessfully();
        return;
    }
    _state = State::WaitingForDestruction;
    startDeadline(QStringLiteral("waiting for the production Settings dialog destruction"));
    connect(_settingsDialog, &QObject::destroyed, this, [this] {
        _deadlineTimer.stop();
        _settingsDialog = nullptr;
        finishSuccessfully();
    });
    _settingsDialog->close();
    _settingsDialog->deleteLater();
}

void NativeScreenshotController::finishSuccessfully()
{
    auto error = QString{};
    if (!_output.completeNativeRun(_runId, &error)) {
        fail(error);
        return;
    }
    _state = State::Finished;
    _finished = true;
    qCInfo(UiScreenshots::lcUiScreenshots) << "Native screenshot phase completed successfully";
    emit finished(EXIT_SUCCESS);
}

void NativeScreenshotController::startDeadline(const QString &operation)
{
    _deadlineOperation = operation;
    _deadlineTimer.start(operationTimeoutMilliseconds);
}

void NativeScreenshotController::fail(const QString &message)
{
    if (_finished) {
        return;
    }
    _finished = true;
    _state = State::Finished;
    _deadlineTimer.stop();
    qCCritical(UiScreenshots::lcUiScreenshots) << message;
    const auto settingsDialog = _settingsDialog.data();
    if (_widget && _widget != settingsDialog) {
        _widget->close();
        _widget->deleteLater();
    }
    if (settingsDialog) {
        settingsDialog->close();
        settingsDialog->deleteLater();
    }
    qCCritical(UiScreenshots::lcUiScreenshots) << "Native screenshot phase failed";
    emit finished(EXIT_FAILURE);
}

QString NativeScreenshotController::currentOutputName() const
{
    if (_job == Job::Finished) {
        return QStringLiteral("native phase completion");
    }
    return UiScreenshots::nativeScreenshotOutputName(_job);
}

}
