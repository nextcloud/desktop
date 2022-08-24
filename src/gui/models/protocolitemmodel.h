/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#pragma once

#include <QAbstractTableModel>

#include "protocolitem.h"

#include "common/fixedsizeringbuffer.h"


namespace OCC {

class ProtocolItemModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum class ProtocolItemRole {
        Action,
        File,
        Folder,
        Size,
        Account,
        Time,
        Status,

        ColumnCount
    };
    Q_ENUM(ProtocolItemRole)

    /**
     * @brief ProtocolItemModel
     * @param parent
     * @param issueMode Whether we are tracking all synced items or issues
     */
    ProtocolItemModel(size_t size, bool issueMode, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addProtocolItem(ProtocolItem &&item);
    const ProtocolItem &protocolItem(const QModelIndex &index) const;

    bool isModelFull() const
    {
        return _data.isFull();
    }

    /**
     * Return underlying unordered raw data
     */
    auto rawData() const
    {
        return _data;
    }

    void reset(std::vector<ProtocolItem> &&data);

    void remove_if(const std::function<bool(const ProtocolItem &)> &filter);

private:
    FixedSizeRingBuffer<ProtocolItem> _data;
    bool _issueMode;
    int _maxLogSize;

};

}
