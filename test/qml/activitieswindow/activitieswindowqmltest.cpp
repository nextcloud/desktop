/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tray/svgimageprovider.h"

#include "logger.h"

#include <QStandardPaths>
#include <QQmlEngine>
#include <QResource>
#include <QtQuickTest>

/** @brief Provides production image resources to the Activities window QML tests. */
class ActivitiesWindowQmlTestSetup : public QObject
{
    Q_OBJECT

public:
    /** @brief Initializes the theme resource containing the SVG icons. */
    ActivitiesWindowQmlTestSetup()
    {
        Q_INIT_RESOURCE(theme);
    }

public slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    /** @brief Adds the production SVG image provider to the test engine. */
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImageProvider(QStringLiteral("svgimage-custom-color"), new OCC::Ui::SvgImageProvider);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(activitieswindow, ActivitiesWindowQmlTestSetup)

#include "activitieswindowqmltest.moc"
