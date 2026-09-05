/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

class ActivitiesMenuQmlTestSetup : public QObject
{
    Q_OBJECT

public:
    ActivitiesMenuQmlTestSetup()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
    }

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The Systray and UserModel singletons are provided as QML mocks so that the
        // menu can be exercised without a running application.
        engine->addImportPath(QStringLiteral(ACTIVITIES_MENU_QML_TEST_IMPORT_PATH));
        engine->addImportPath(QStringLiteral("qrc:/qml/theme"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(activitiesmenu, ActivitiesMenuQmlTestSetup)

#include "activitiesmenuqmltestrunner.moc"
