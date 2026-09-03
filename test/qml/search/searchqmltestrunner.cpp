/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include "gui/search/unifiedsearchpeoplemodel.h"
#include "gui/search/unifiedsearchresultslistmodel.h"

class SearchQmlTestSetup : public QObject
{
    Q_OBJECT

public:
    SearchQmlTestSetup()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);

        qmlRegisterType<OCC::UnifiedSearchPeopleModel>("com.nextcloud.desktopclient", 1, 0, "UnifiedSearchPeopleModel");
        qmlRegisterUncreatableType<OCC::UnifiedSearchResultsListModel>("com.nextcloud.desktopclient",
                                                                       1,
                                                                       0,
                                                                       "UnifiedSearchResultsListModel",
                                                                       "UnifiedSearchResultsListModel");
    }

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral(SEARCH_QML_TEST_IMPORT_PATH));
        engine->addImportPath(QCoreApplication::applicationDirPath());
        engine->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
        engine->addImportPath(QStringLiteral("qrc:/qml/theme"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(search, SearchQmlTestSetup)

#include "searchqmltestrunner.moc"
