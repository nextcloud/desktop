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

public:
    DeclarativeUi(QObject *parent = nullptr);

    void setAccountState(AccountState *accountState);
    void setLocalPath(const QString &localPath);

    [[nodiscard]] AccountState *accountState() const;
    [[nodiscard]] QString localPath() const;
    [[nodiscard]] DeclarativeUiModel *declarativeUiModel() const;

signals:
    void declarativeUiFetched();
    void endpointsParsed();
    void localPathChanged();
    void accountStateChanged();
    void declarativeUiModelChanged();

private:
    AccountState *_accountState;
    QString _localPath;

    std::unique_ptr<DeclarativeUiModel> _declarativeUiModel;
};

}
