/*
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include "platform_win.h"

#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMetaMethod>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWinExtras/qwinfunctions.h>
#endif
#include <qt_windows.h>

#include <chrono>
#include <thread>

#include "theme.h"

#include "common/asserts.h"

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

struct
{
    HANDLE windowMessageWatcherEvent = CreateEventW(nullptr, true, false, nullptr);
    bool windowMessageWatcherRun = true;
    std::thread *watcherThread = nullptr;
} watchWMCtx;

} // anonymous namespace

namespace OCC {

Q_LOGGING_CATEGORY(lcPlatform, "platform.windows")

WinPlatform::WinPlatform()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

WinPlatform::~WinPlatform()
{
}

void WinPlatform::setApplication(QCoreApplication *application)
{
    // Ensure OpenSSL config file is only loaded from app directory
    const QString opensslConf = QCoreApplication::applicationDirPath() + QStringLiteral("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());

    // The Windows style still has pixelated elements with Qt 5.6,
    // it's recommended to use the Fusion style in this case, even
    // though it looks slightly less native. Check here after the
    // QApplication was constructed, but before any QWidget is
    // constructed.
    if (auto guiApp = qobject_cast<QGuiApplication *>(application)) {
        if (guiApp->devicePixelRatio() > 1) {
            QApplication::setStyle(QStringLiteral("fusion"));
        }
    }
}

void OCC::WinPlatform::startServices()
{
    startShutdownWatcher();
}

void WinPlatform::startShutdownWatcher()
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        icon = QtWin::toHICON(Theme::instance()->applicationIcon().pixmap(64, 64));
#else
        icon = Theme::instance()->applicationIcon().pixmap(64, 64).toImage().toHICON();
#endif
    }
    watchWMCtx.watcherThread = new std::thread([icon] {
        WNDCLASS wc = {};
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"ocWindowMessageWatcher";
        wc.hIcon = icon;
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            //            qDebug() << MSG { hwnd, msg, wParam, lParam, 0, {} };
            if (msg == WM_QUERYENDSESSION) {
                qCDebug(lcPlatform) << "Received WM_QUERYENDSESSION";
                return 1;
            } else if (msg == WM_ENDSESSION) {
                qCDebug(lcPlatform) << "Received WM_ENDSESSION quitting";
                QMetaObject::invokeMethod(qApp, &QApplication::quit);
                auto start = steady_clock::now();
                if (lParam == ENDSESSION_LOGOFF) {
                    // block the windows shutdown until we are done
                    const QString description = QApplication::translate("Utility", "Shutting down %1").arg(Theme::instance()->appNameGUI());
                    qCDebug(lcPlatform) << "Block shutdown until we are ready" << description;
                    OC_ASSERT(ShutdownBlockReasonCreate(hwnd, reinterpret_cast<const wchar_t *>(description.utf16())));
                }
                WaitForSingleObject(watchWMCtx.windowMessageWatcherEvent, INFINITE);
                if (lParam == ENDSESSION_LOGOFF) {
                    OC_ASSERT(ShutdownBlockReasonDestroy(hwnd));
                }
                qCInfo(lcPlatform) << "WM_ENDSESSION successfully shut down" << (steady_clock::now() - start);
                watchWMCtx.windowMessageWatcherRun = false;
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        };
        OC_ASSERT(RegisterClass(&wc));

        auto watcherWindow = CreateWindowW(wc.lpszClassName, reinterpret_cast<const wchar_t *>(Theme::instance()->appNameGUI().utf16()), WS_OVERLAPPED,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wc.hInstance, nullptr);
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

} // namespace OCC
