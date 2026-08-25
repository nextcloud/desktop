/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>

#include "unifiedshare.h"

namespace OCC::Gui::Sharing {

/**
 * @brief Base class for list models that expose one category of a share's details.
 *
 * Derived classes expose details such as recipients, permissions, or properties
 * from the current share. Changing the share resets the model. The model does
 * not own the share, so callers must keep it alive while it is assigned.
 */
class ShareDetailsListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(Share *share READ share WRITE setShare NOTIFY shareChanged)

public:
    /** @brief Constructs an empty share-details list model. */
    explicit ShareDetailsListModel(QObject *parent = nullptr);

    /** @brief Returns the share whose details this model exposes, or `nullptr` when none is assigned. */
    [[nodiscard]] Share* share() const;
    /**
     * @brief Sets the share whose details this model exposes.
     *
     * The model resets when the assigned share changes. The model does not take
     * ownership of `share`.
     */
    virtual void setShare(Share* share);

Q_SIGNALS:
    /** @brief Emitted after the assigned share changes. */
    void shareChanged();

protected:
    Share *_share = nullptr; //!< The non-owning share whose details this model exposes.
};

}
