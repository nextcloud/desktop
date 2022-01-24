/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#ifndef INTERNALLINKWIDGET_H
#define INTERNALLINKWIDGET_H

#include "QProgressIndicator.h"
#include <QList>
#include <QPushButton>

#include "ui_internallinkwidget.h"

namespace OCC {

/**
 * @brief The ShareDialog class
 * @ingroup gui
 */
class InternalLinkWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InternalLinkWidget(const QString &localPath,
        QWidget *parent = nullptr);
    ~InternalLinkWidget() override = default;

    void setupUiOptions();

public slots:
    void slotStyleChanged();

private slots:
    void slotLinkFetched(const QString &url);
    void slotCopyInternalLink() const;

private:
    void customizeStyle();

    std::unique_ptr<Ui::InternalLinkWidget> _ui = std::make_unique<Ui::InternalLinkWidget>();
    QString _localPath;
    QString _internalUrl;

    QPushButton *_copyInternalLinkButton{};
};
}

#endif // INTERNALLINKWIDGET_H
