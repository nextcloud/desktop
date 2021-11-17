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

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QUrlQuery>
#include <QIcon>

#ifdef Q_OS_WIN
#include <QThread>
#include <QTimer>
#endif

#include "theme.h"

#include "common/asserts.h"

namespace OCC {
Q_LOGGING_CATEGORY(lcGuiUtility, "gui.utility", QtInfoMsg)
}

using namespace OCC;

namespace {

#ifdef Q_OS_WIN
void watchWM()
{
    // Qt only receives window message if a window was displayed at least once
    // create an invisible window to handle WM_ENDSESSION
    QThread::create([] {
        WNDCLASS wc = {};
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"ocWindowMessageWatcher";
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            //            qDebug() << MSG { hwnd, msg, wParam, lParam, 0, {} };
            if (msg == WM_QUERYENDSESSION) {
                return 1;
            } else if (msg == WM_ENDSESSION) {
                qCInfo(OCC::lcUtility) << "Received WM_ENDSESSION quitting";
                QTimer::singleShot(0, qApp, &QApplication::quit);
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        };

        OC_ASSERT(RegisterClass(&wc));

        auto window = CreateWindowW(wc.lpszClassName, L"watcher", WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wc.hInstance, nullptr);
        OC_ASSERT_X(window, Utility::formatWinError(GetLastError()).toUtf8().constData());

        bool run = true;
        QObject::connect(qApp, &QApplication::aboutToQuit, [&run] {
            run = false;
        });
        MSG msg;
        while (run) {
            if (!PeekMessageW(&msg, window, 0, 0, PM_REMOVE)) {
                QThread::msleep(100);
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    })->start();
}
Q_COREAPP_STARTUP_FUNCTION(watchWM);
#endif
}

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
    QUrl url(QLatin1String("mailto:"));
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
    const QString path = Theme::instance()->isUsingDarkTheme() ? QStringLiteral("dark") : QStringLiteral("light");
    const QIcon icon(QStringLiteral(":/client/resources/%1/%2").arg(path, icon_name));
    Q_ASSERT(!icon.isNull());
    return icon;
}
