/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef ACTIVITYDATA_H
#define ACTIVITYDATA_H

#include <QtCore>
#include <QIcon>
#include <QJsonObject>

#include "syncfileitem.h"
#include "folder.h"
#include "account.h"

namespace OCC {
/**
 * @brief The ActivityLink class describes actions of an activity
 *
 * These are part of notifications which are mapped into activities.
 */

class ActivityLink
{
    Q_GADGET
    
    Q_PROPERTY(QString imageSource MEMBER _imageSource)
    Q_PROPERTY(QString imageSourceHovered MEMBER _imageSourceHovered)
    Q_PROPERTY(QString label MEMBER _label)
    Q_PROPERTY(QString link MEMBER _link)
    Q_PROPERTY(QByteArray verb MEMBER _verb)
    Q_PROPERTY(bool primary MEMBER _primary)

public:
    static ActivityLink createFomJsonObject(const QJsonObject &obj);

public:
    QString _imageSource;
    QString _imageSourceHovered;
    QString _label;
    QString _link;
    QByteArray _verb;
    bool _primary;
};

/**
 * @brief The PreviewData class describes the data about a file's preview.
 */

class PreviewData
{
    Q_GADGET

    Q_PROPERTY(QString source MEMBER _source)
    Q_PROPERTY(QString link MEMBER _link)
    Q_PROPERTY(QString mimeType MEMBER _mimeType)
    Q_PROPERTY(int fileId MEMBER _fileId)
    Q_PROPERTY(QString view MEMBER _view)
    Q_PROPERTY(bool isMimeTypeIcon MEMBER _isMimeTypeIcon)
    Q_PROPERTY(QString filename MEMBER _filename)

public:
    QString _source;
    QString _link;
    QString _mimeType;
    int _fileId;
    QString _view;
    bool _isMimeTypeIcon;
    QString _filename;
};

/* ==================================================================== */
/**
 * @brief Activity Structure
 * @ingroup gui
 *
 * contains all the information describing a single activity.
 */

class Activity
{
public:
    using Identifier = QPair<qlonglong, QString>;

    enum Type {
        ActivityType,
        NotificationType,
        SyncResultType,
        SyncFileItemType
    };

    static Activity fromActivityJson(const QJsonObject &json, const AccountPtr account);

    static QString relativeServerFileTypeIconPath(const QMimeType &mimeType);

    struct RichSubjectParameter {
        QString type;    // Required
        QString id;      // Required
        QString name;    // Required
        QString path;    // Required (for files only)
        QUrl link;    // Optional (files only)
    };

    struct TalkNotificationData {
        QString conversationToken;
        QString messageId;
        QString messageSent;
    };

    Type _type;
    qlonglong _id;
    QString _fileAction;
    int _objectId;
    TalkNotificationData _talkNotificationData;
    QString _objectType;
    QString _objectName;
    QString _subject;
    QString _subjectRich;
    QHash<QString, RichSubjectParameter> _subjectRichParameters;
    QString _subjectDisplay;
    QString _message;
    QString _folder;
    QString _file;
    QString _renamedFile;
    QUrl _link;
    QDateTime _dateTime;
    qint64 _expireAtMsecs = -1;
    QString _accName;
    QString _darkIcon;
    QString _lightIcon;
    bool _isCurrentUserFileActivity = false;
    QVector<PreviewData> _previews;

    // Stores information about the error
    int _status;

    QVector<ActivityLink> _links;
    /**
     * @brief Sort operator to sort the list youngest first.
     * @param val
     * @return
     */


    Identifier ident() const;
};

bool operator==(const Activity &rhs, const Activity &lhs);
bool operator<(const Activity &rhs, const Activity &lhs);

/* ==================================================================== */
/**
 * @brief The ActivityList
 * @ingroup gui
 *
 * A QList based list of Activities
 */
using ActivityList = QList<Activity>;
}

Q_DECLARE_METATYPE(OCC::Activity::Type)
Q_DECLARE_METATYPE(OCC::ActivityLink)
Q_DECLARE_METATYPE(OCC::PreviewData)

#endif // ACTIVITYDATA_H
