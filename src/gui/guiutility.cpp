/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "guiutility.h"
#include "application.h"
#include "settingsdialog.h"

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QUrlQuery>
#include <QIcon>

#ifdef Q_OS_WIN
#include <QMetaMethod>

#include <QtWinExtras/qwinfunctions.h>
#include <qt_windows.h>

#include <chrono>
#include <thread>

using namespace std::chrono;
using namespace std::chrono_literals;
#endif

#include "theme.h"

#include "common/asserts.h"

namespace OCC {
Q_LOGGING_CATEGORY(lcGuiUtility, "gui.utility", QtInfoMsg)
}

using namespace OCC;

#ifdef Q_OS_WIN
namespace {

// TODO: 2.11 move to the new Platform class
struct
{
    HANDLE windowMessageWatcherEvent = CreateEventW(nullptr, true, false, nullptr);
    bool windowMessageWatcherRun = true;
    std::thread *watcherThread = nullptr;
} watchWMCtx;
}

void Utility::startShutdownWatcher()
{
    if (watchWMCtx.watcherThread) {
        return;
    }
    // Qt only receives window message if a window was displayed at least once
    // create an invisible window to handle WM_ENDSESSION
    // We also block a system shutdown until we are properly shutdown our selfs
    // In the unlikely case that we require more than 5s Windows will require a fullscreen message
    // with our icon, title and the reason why we are blocking the shutdown.

    // ensure to initialise the icon in the main thread
    HICON icon = {};
    if (qobject_cast<QGuiApplication *>(qApp)) {
        icon = QtWin::toHICON(Theme::instance()->applicationIcon().pixmap(64, 64));
    }
    watchWMCtx.watcherThread = new std::thread([icon] {
        WNDCLASS wc = {};
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"ocWindowMessageWatcher";
        wc.hIcon = icon;
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            //            qDebug() << MSG { hwnd, msg, wParam, lParam, 0, {} };
            if (msg == WM_QUERYENDSESSION) {
                qCDebug(OCC::lcUtility) << "Received WM_QUERYENDSESSION";
                return 1;
            } else if (msg == WM_ENDSESSION) {
                qCDebug(OCC::lcUtility) << "Received WM_ENDSESSION quitting";
                QMetaObject::invokeMethod(qApp, &QApplication::quit);
                auto start = steady_clock::now();
                if (lParam == ENDSESSION_LOGOFF) {
                    // block the windows shutdown until we are done
                    const QString description = QApplication::translate("Utility", "Shutting down %1").arg(Theme::instance()->appNameGUI());
                    qCDebug(OCC::lcUtility) << "Block shutdown until we are ready" << description;
                    OC_ASSERT(ShutdownBlockReasonCreate(hwnd, reinterpret_cast<const wchar_t *>(description.utf16())));
                }
                WaitForSingleObject(watchWMCtx.windowMessageWatcherEvent, INFINITE);
                if (lParam == ENDSESSION_LOGOFF) {
                    OC_ASSERT(ShutdownBlockReasonDestroy(hwnd));
                }
                qCInfo(OCC::lcUtility) << "WM_ENDSESSION successfully shut down" << (steady_clock::now() - start);
                watchWMCtx.windowMessageWatcherRun = false;
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        };
        OC_ASSERT(RegisterClass(&wc));

        auto watcherWindow = CreateWindowW(wc.lpszClassName, reinterpret_cast<const wchar_t *>(Theme::instance()->appNameGUI().utf16()),
            WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wc.hInstance, nullptr);
        OC_ASSERT_X(watcherWindow, Utility::formatWinError(GetLastError()).toUtf8().constData());

        MSG msg;
        while (watchWMCtx.windowMessageWatcherRun) {
            if (!PeekMessageW(&msg, watcherWindow, 0, 0, PM_REMOVE)) {
                std::this_thread::sleep_for(100ms);
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    });

    qAddPostRoutine([] {
        qCDebug(OCC::lcUtility) << "app closed";
        SetEvent(watchWMCtx.windowMessageWatcherEvent);
    });
}
#endif


bool Utility::openBrowser(const QUrl &url, QWidget *errorWidgetParent)
{
    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(
                errorWidgetParent,
                QCoreApplication::translate("utility", "Could not open browser"),
                QCoreApplication::translate("utility",
                    "There was an error when launching the browser to go to "
                    "URL %1. Maybe no default browser is configured?")
                    .arg(url.toString()));
        }
        qCWarning(lcGuiUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

bool Utility::openEmailComposer(const QString &subject, const QString &body, QWidget *errorWidgetParent)
{
    QUrl url(QStringLiteral("mailto:"));
    QUrlQuery query;
    query.setQueryItems({ { QLatin1String("subject"), subject },
        { QLatin1String("body"), body } });
    url.setQuery(query);

    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(
                errorWidgetParent,
                QCoreApplication::translate("utility", "Could not open email client"),
                QCoreApplication::translate("utility",
                    "There was an error when launching the email client to "
                    "create a new message. Maybe no default email client is "
                    "configured?"));
        }
        qCWarning(lcGuiUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

QString Utility::vfsCurrentAvailabilityText(VfsItemAvailability availability)
{
    switch(availability) {
    case VfsItemAvailability::AlwaysLocal:
        return QCoreApplication::translate("utility", "Always available locally");
    case VfsItemAvailability::AllHydrated:
        return QCoreApplication::translate("utility", "Currently available locally");
    case VfsItemAvailability::Mixed:
        return QCoreApplication::translate("utility", "Some available online only");
    case VfsItemAvailability::AllDehydrated:
        return QCoreApplication::translate("utility", "Available online only");
    case VfsItemAvailability::OnlineOnly:
        return QCoreApplication::translate("utility", "Available online only");
    }
    Q_UNREACHABLE();
}

QString Utility::vfsPinActionText()
{
    return QCoreApplication::translate("utility", "Make always available locally");
}

QString Utility::vfsFreeSpaceActionText()
{
    return QCoreApplication::translate("utility", "Free up local space");
}


QIcon Utility::getCoreIcon(const QString &icon_name)
{
    if (icon_name.isEmpty()) {
        return {};
    }
    const QString theme = Theme::instance()->isUsingDarkTheme() ? QStringLiteral("dark") : QStringLiteral("light");
    const QString path = QStringLiteral(":/client/resources/%1/%2").arg(theme, icon_name);
    const QIcon icon(path);
    // were we able to load the file?
    Q_ASSERT(icon.actualSize({ 100, 100 }).isValid());
    return icon;
}


void Utility::setModal(QWidget *w)
{
    // setting both sheet and explicitly modal
    // can cause window stacking issues
#ifdef Q_OS_MAC
    w->setWindowFlags(Qt::Sheet);
#else
    w->setWindowModality(Qt::ApplicationModal);
#endif
    w->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
}
