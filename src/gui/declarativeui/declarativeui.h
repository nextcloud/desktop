/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLoggingCategory>
#include <QObject>

#include "accountstate.h"
#include "declarativeuimodel.h"
#include "endpointmodel.h"

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcDeclarativeUi)
class JsonApiJob;

class DeclarativeUi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AccountState* accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY localPathChanged)
    Q_PROPERTY(DeclarativeUiModel* declarativeUiModel READ declarativeUiModel NOTIFY declarativeUiModelChanged)
    Q_PROPERTY(EndpointModel* endpointModel READ endpointModel NOTIFY endpointModelChanged)

public:
    DeclarativeUi(QObject *parent = nullptr);

    void fetchEndpoints();

    void setAccountState(AccountState *accountState);
    void setLocalPath(const QString &localPath);

    [[nodiscard]] AccountState *accountState() const;
    [[nodiscard]] QString localPath() const;
    [[nodiscard]] DeclarativeUiModel *declarativeUiModel() const;
    [[nodiscard]] EndpointModel *endpointModel() const;

signals:
    void declarativeUiFetched();
    void localPathChanged();
    void accountStateChanged();
    void declarativeUiModelChanged();
    void endpointModelChanged();

private:
    AccountState *_accountState;
    QString _localPath;

    std::unique_ptr<DeclarativeUiModel> _declarativeUiModel;
    std::unique_ptr<EndpointModel> _endpointModel;
};

}
