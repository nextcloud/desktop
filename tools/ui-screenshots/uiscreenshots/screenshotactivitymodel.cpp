/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotactivitymodel.h"

namespace OCC {

namespace {
enum ActivityRole : int {
    IconRole = Qt::UserRole + 1,
    AccountRole,
    ObjectTypeRole,
    ObjectIdRole,
    ObjectNameRole,
    ActionsLinksRole,
    ActionsLinksContextMenuRole,
    ActionsLinksForActionButtonsRole,
    ActionTextRole,
    ActionTextColorRole,
    ActionRole,
    MessageRole,
    DisplayPathRole,
    PathRole,
    OpenablePathRole,
    DisplayLocationRole,
    LinkRole,
    PointInTimeRole,
    AccountConnectedRole,
    DisplayActionsRole,
    ShowFileDetailsRole,
    ShareableRole,
    DismissableRole,
    IsCurrentUserFileActivityRole,
    ThumbnailRole,
    ConversationTokenRole,
    MessageIdRole,
    MessageSentRole,
    UserAvatarRole,
    ActivityIndexRole,
    ActivityDataRole,
    ServerHasIntegrationRole,
};

struct FixtureActivity
{
    QString type;
    QString objectType;
    QString subject;
    QString message;
    QString dateTime;
    QString icon;
};

const QVector<FixtureActivity> activities{
    {
        QStringLiteral("Activity"),
        QStringLiteral("files"),
        QStringLiteral("Project brief.docx was shared with you"),
        QStringLiteral("Alex Morgan shared Project brief.docx"),
        QStringLiteral("Just now"),
        QStringLiteral("image://svgimage-custom-color/share.svg"),
    },
    {
        QStringLiteral("File"),
        QStringLiteral("files"),
        QStringLiteral("Design notes.md was synchronized"),
        QStringLiteral("Design notes.md was synchronized"),
        QStringLiteral("5 minutes ago"),
        QStringLiteral("image://svgimage-custom-color/change.svg"),
    },
};
}

ScreenshotActivityModel::ScreenshotActivityModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScreenshotActivityModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : activities.size();
}

QVariant ScreenshotActivityModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= activities.size()) {
        return {};
    }
    const auto &activity = activities.at(index.row());
    switch (role) {
    case IconRole:
        return activity.icon;
    case AccountRole:
        return QStringLiteral("Alex Morgan@cloud.example.com");
    case ObjectTypeRole:
        return activity.objectType;
    case ObjectIdRole:
        return QString::number(index.row() + 1);
    case ObjectNameRole:
        return index.row() == 0 ? QStringLiteral("Project brief.docx") : QStringLiteral("Design notes.md");
    case ActionsLinksRole:
    case ActionsLinksContextMenuRole:
    case ActionsLinksForActionButtonsRole:
        return QVariantList{};
    case ActionTextRole:
        return activity.subject;
    case ActionTextColorRole:
        return QStringLiteral("#222222");
    case ActionRole:
        return activity.type;
    case MessageRole:
        return activity.message;
    case DisplayPathRole:
        return QStringLiteral("Documents");
    case PathRole:
    case OpenablePathRole:
    case LinkRole:
    case ConversationTokenRole:
    case MessageIdRole:
    case MessageSentRole:
    case UserAvatarRole:
        return QString{};
    case DisplayLocationRole:
        return QStringLiteral("Documents");
    case PointInTimeRole:
        return activity.dateTime;
    case AccountConnectedRole:
        return true;
    case DisplayActionsRole:
    case ShowFileDetailsRole:
    case ShareableRole:
    case DismissableRole:
    case IsCurrentUserFileActivityRole:
    case ServerHasIntegrationRole:
        return false;
    case ThumbnailRole:
        return {};
    case ActivityIndexRole:
        return index.row();
    case ActivityDataRole:
        return QVariantMap{
            {QStringLiteral("type"), activity.type},
            {QStringLiteral("subject"), activity.subject},
            {QStringLiteral("message"), activity.message},
        };
    }
    return {};
}

quint32 ScreenshotActivityModel::maxActionButtons() const
{
    return 3;
}

bool ScreenshotActivityModel::hasSyncConflicts() const
{
    return false;
}

QVariantList ScreenshotActivityModel::allConflicts() const
{
    return {};
}

void ScreenshotActivityModel::slotTriggerDefaultAction(const int activityIndex)
{
    Q_UNUSED(activityIndex)
}

void ScreenshotActivityModel::slotTriggerAction(const int activityIndex, const int actionIndex)
{
    Q_UNUSED(activityIndex)
    Q_UNUSED(actionIndex)
}

void ScreenshotActivityModel::slotTriggerDismiss(const int activityIndex)
{
    Q_UNUSED(activityIndex)
}

void ScreenshotActivityModel::sendReplyMessage(const int activityIndex, const QString &conversationToken, const QString &message, const QString &replyTo)
{
    Q_UNUSED(activityIndex)
    Q_UNUSED(conversationToken)
    Q_UNUSED(message)
    Q_UNUSED(replyTo)
}

QHash<int, QByteArray> ScreenshotActivityModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {IconRole, QByteArrayLiteral("icon")},
        {AccountRole, QByteArrayLiteral("account")},
        {ObjectTypeRole, QByteArrayLiteral("objectType")},
        {ObjectIdRole, QByteArrayLiteral("objectId")},
        {ObjectNameRole, QByteArrayLiteral("objectName")},
        {ActionsLinksRole, QByteArrayLiteral("links")},
        {ActionsLinksContextMenuRole, QByteArrayLiteral("linksContextMenu")},
        {ActionsLinksForActionButtonsRole, QByteArrayLiteral("linksForActionButtons")},
        {ActionTextRole, QByteArrayLiteral("subject")},
        {ActionTextColorRole, QByteArrayLiteral("activityTextTitleColor")},
        {ActionRole, QByteArrayLiteral("type")},
        {MessageRole, QByteArrayLiteral("message")},
        {DisplayPathRole, QByteArrayLiteral("displayPath")},
        {PathRole, QByteArrayLiteral("path")},
        {OpenablePathRole, QByteArrayLiteral("openablePath")},
        {DisplayLocationRole, QByteArrayLiteral("displayLocation")},
        {LinkRole, QByteArrayLiteral("link")},
        {PointInTimeRole, QByteArrayLiteral("dateTime")},
        {AccountConnectedRole, QByteArrayLiteral("accountConnected")},
        {DisplayActionsRole, QByteArrayLiteral("displayActions")},
        {ShowFileDetailsRole, QByteArrayLiteral("showFileDetails")},
        {ShareableRole, QByteArrayLiteral("isShareable")},
        {DismissableRole, QByteArrayLiteral("isDismissable")},
        {IsCurrentUserFileActivityRole, QByteArrayLiteral("isCurrentUserFileActivity")},
        {ThumbnailRole, QByteArrayLiteral("thumbnail")},
        {ConversationTokenRole, QByteArrayLiteral("conversationToken")},
        {MessageIdRole, QByteArrayLiteral("messageId")},
        {MessageSentRole, QByteArrayLiteral("messageSent")},
        {UserAvatarRole, QByteArrayLiteral("userAvatar")},
        {ActivityIndexRole, QByteArrayLiteral("activityIndex")},
        {ActivityDataRole, QByteArrayLiteral("activity")},
        {ServerHasIntegrationRole, QByteArrayLiteral("serverHasIntegration")},
    };
    return roles;
}

}
