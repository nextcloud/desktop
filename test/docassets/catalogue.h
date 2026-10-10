/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace OCC::DocAssets
{
inline const auto scenarioId = QStringLiteral("wizard-server");
inline const auto screenshotFilename = QStringLiteral("wizard-server.png");
inline const auto windowSource = QUrl(QStringLiteral("qrc:/qml/src/gui/wizard/qml/AccountWizardWindow.qml"));
inline const auto fixtureServer = QStringLiteral("https://cloud.example.com");
inline constexpr auto captureTimeoutMs = 15000;
inline constexpr auto workerStartupTimeoutMs = 10000;
inline constexpr auto captureSkippedExitCode = 77;
QMap<QString, QString> statusIconSources();
QStringList captureScenarios();
QStringList captureOmissions();
QString captureFilename(const QString &scenario);
}
