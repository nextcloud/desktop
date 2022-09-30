/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include <QObject>
#include <QSettings>
#include <QObject>
#include <QQmlEngine>
#include <QVariant>
#include <QVector>
#include <QAbstractItemModel>

namespace OCC {

struct Emoji
{
    Emoji(QString u, QString s, bool isCustom = false)
        : unicode(std::move(std::move(u)))
        , shortname(std::move(std::move(s)))
        , isCustom(isCustom)
    {
    }
    Emoji() = default;

    friend QDataStream &operator<<(QDataStream &arch, const Emoji &object)
    {
        arch << object.unicode;
        arch << object.shortname;
        return arch;
    }

    friend QDataStream &operator>>(QDataStream &arch, Emoji &object)
    {
        arch >> object.unicode;
        arch >> object.shortname;
        object.isCustom = object.unicode.startsWith("image://");
        return arch;
    }

    QString unicode;
    QString shortname;
    bool isCustom = false;

    Q_GADGET
    Q_PROPERTY(QString unicode MEMBER unicode)
    Q_PROPERTY(QString shortname MEMBER shortname)
    Q_PROPERTY(bool isCustom MEMBER isCustom)
};

class EmojiCategoriesModel : public QAbstractListModel
{
public:
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    enum Roles {
        EmojiRole = 0,
        LabelRole
    };

    struct Category
    {
        QString emoji;
        QString label;
    };

    static const std::vector<Category> categories;
};

class EmojiModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList model READ model NOTIFY modelChanged)
    Q_PROPERTY(QAbstractListModel *emojiCategoriesModel READ emojiCategoriesModel CONSTANT)

    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

    Q_PROPERTY(QVariantList people MEMBER people CONSTANT)
    Q_PROPERTY(QVariantList nature MEMBER nature CONSTANT)
    Q_PROPERTY(QVariantList food MEMBER food CONSTANT)
    Q_PROPERTY(QVariantList activity MEMBER activity CONSTANT)
    Q_PROPERTY(QVariantList travel MEMBER travel CONSTANT)
    Q_PROPERTY(QVariantList objects MEMBER objects CONSTANT)
    Q_PROPERTY(QVariantList symbols MEMBER symbols CONSTANT)
    Q_PROPERTY(QVariantList flags MEMBER flags CONSTANT)

public:
    explicit EmojiModel(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE [[nodiscard]] QVariantList history() const;
    Q_INVOKABLE void setCategory(const QString &category);
    Q_INVOKABLE void emojiUsed(const QVariant &modelData);

    [[nodiscard]] QVariantList model() const;
    QAbstractListModel *emojiCategoriesModel();

signals:
    void historyChanged();
    void modelChanged();

private:
    static const QVariantList people;
    static const QVariantList nature;
    static const QVariantList food;
    static const QVariantList activity;
    static const QVariantList travel;
    static const QVariantList objects;
    static const QVariantList symbols;
    static const QVariantList flags;

    QSettings _settings;
    QString _category = "history";

    EmojiCategoriesModel _emojiCategoriesModel;
};

}

Q_DECLARE_METATYPE(OCC::Emoji)
