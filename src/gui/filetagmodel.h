/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>

#include "libsync/account.h"

namespace OCC {

class FileTagModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString serverRelativePath READ serverRelativePath WRITE setServerRelativePath NOTIFY serverRelativePathChanged)
    Q_PROPERTY(AccountPtr account READ account WRITE setAccount NOTIFY accountChanged)
    Q_PROPERTY(int maxTags READ maxTags WRITE setMaxTags NOTIFY maxTagsChanged)
    Q_PROPERTY(int totalTags READ totalTags NOTIFY totalTagsChanged)
    Q_PROPERTY(QString overflowTagsString READ overflowTagsString NOTIFY overflowTagsStringChanged)

public:
    explicit FileTagModel(const QString &serverRelativePath,
                          const AccountPtr &account,
                          QObject *const parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    [[nodiscard]] QString serverRelativePath() const;
    [[nodiscard]] AccountPtr account() const;

    [[nodiscard]] int maxTags() const;
    [[nodiscard]] int totalTags() const;

    [[nodiscard]] QString overflowTagsString() const;

signals:
    void serverRelativePathChanged();
    void accountChanged();

    void maxTagsChanged();
    void totalTagsChanged();
    void overflowTagsStringChanged();

public slots:
    void setServerRelativePath(const QString &serverRelativePath);
    void setAccount(const OCC::AccountPtr &account);

    void setMaxTags(const int maxTags);
    void updateOverflowTagsString();

    void resetForNewFile();

private slots:
    void fetchFileTags();
    void processFileTagRequestFinished(const QVariantMap &result);
    void processFileTagRequestFinishedWithError();

private:
    QString _serverRelativePath;
    AccountPtr _account;
    QStringList _tags;

    int _maxTags = 0;
    QString _overflowTagsString;
};

}
