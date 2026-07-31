/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>

#include "share.h"

namespace OCC::Gui::Sharing {

class AbstractShareModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(Share *share READ share WRITE setShare NOTIFY shareChanged)

public:
    explicit AbstractShareModel(QObject *parent = nullptr);

    [[nodiscard]] Share* share() const;
    virtual void setShare(Share* share);

Q_SIGNALS:
    void shareChanged();

protected:
    Share *_share = nullptr;
};

}
