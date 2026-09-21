/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchfixtures.h"
#include "capturescene.h"
#include "fixtureaccount.h"
#include "search/unifiedsearchresultslistmodel.h"

namespace OCC::DocAssets
{
bool prepareSearch(CaptureScene &scene)
{
    prepareAccount(scene);
    auto *model = new UnifiedSearchResultsListModel(scene.accountState.data(), 0, 0, &scene);
    scene.module = QStringLiteral("com.nextcloud.desktopclient.search");
    scene.type = QStringLiteral("SearchWindow");
    scene.properties = {{QStringLiteral("searchModel"), QVariant::fromValue<QObject *>(model)},
                        {QStringLiteral("account"),
                         QVariantMap{{QStringLiteral("name"), QStringLiteral("Alex Morgan")},
                                     {QStringLiteral("server"), QStringLiteral("cloud.example.com")},
                                     {QStringLiteral("avatar"), QStringLiteral("qrc:/client/theme/black/user.svg")}}}};
    model->setSearchTerm(QStringLiteral("Project"));
    scene.ready = [model](QString *failure) {
        if (!model->errorString().isEmpty()) {
            *failure = model->errorString();
        }
        return model->providersReady() && !model->isSearchInProgress() && !model->waitingForSearchTermEditEnd() && model->rowCount() > 0;
    };
    return true;
}
}
