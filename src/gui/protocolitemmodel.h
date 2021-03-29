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

        ColumnCount
    };
    /**
     * @brief ProtocolItemModel
     * @param parent
     * @param issueMode Whether we are tracking all synced items or issues
     */
    ProtocolItemModel(QObject *parent = nullptr, bool issueMode = false, int maxLogSize=2000);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addProtocolItem(const ProtocolItem &&item);
    const ProtocolItem &protocolItem(const QModelIndex &index) const;

    const std::vector<ProtocolItem> &rawData() const;
    void remove(const std::function<bool(const ProtocolItem &)> &filter);

    bool isModelFull() const
    {
        return _data.size() == _maxLogSize;
    }

private:
    bool _issueMode;
    int _maxLogSize;
    std::vector<ProtocolItem> _data;

    qulonglong _start = 0;
    qulonglong _end = 0;

    constexpr qulonglong actualSize() const
    {
        return _end - _start;
    }
    constexpr int convertToIndex(qulonglong i) const
    {
        return (i + _start) % _maxLogSize;
    }
};

}
