/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "declarativeui.h"
#include "networkjobs.h"
#include "accountfwd.h"
#include "account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcDeclarativeUi, "nextcloud.gui.declarativeui", QtInfoMsg)

DeclarativeUi::DeclarativeUi(QObject *parent)
    : QObject(parent)
{
}

void DeclarativeUi::setAccountState(AccountState *accountState)
{
    if (accountState == nullptr) {
        return;
    }

    if (accountState == _accountState) {
        return;
    }

    _accountState = accountState;
    _declarativeUiModel = std::make_unique<DeclarativeUiModel>(_accountState->account(), this);
    connect(_declarativeUiModel.get(), &DeclarativeUiModel::pageFetched,
            this, &DeclarativeUi::declarativeUiFetched);
    connect(this, &DeclarativeUi::declarativeUiFetched,
            this, &DeclarativeUi::declarativeUiModelChanged);

    Q_EMIT accountStateChanged();
}

void DeclarativeUi::setLocalPath(const QString &localPath)
{
    if (localPath.isEmpty()) {
        return;
    }

    if (localPath == _localPath) {
        return;
    }

    _localPath = localPath;
    Q_EMIT localPathChanged();
}

AccountState *DeclarativeUi::accountState() const
{
    return _accountState;
}

QString DeclarativeUi::localPath() const
{
    return _localPath;
}

DeclarativeUiModel *DeclarativeUi::declarativeUiModel() const {
    return _declarativeUiModel.get();
}

}
