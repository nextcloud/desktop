#ifndef NOTIFICATIONHANDLER_H
#define NOTIFICATIONHANDLER_H

#include <QtCore>

#include "UserModel.h"

class QJsonDocument;

namespace OCC {

class ServerNotificationHandler : public QObject
{
    Q_OBJECT
public:
    explicit ServerNotificationHandler(AccountState *accountState, QObject *parent = nullptr);
    static QMap<int, QByteArray> iconCache;

signals:
    void newNotificationList(ActivityList);

public slots:
    void slotFetchNotifications();

private slots:
    void slotNotificationsReceived(const QJsonDocument &json, int statusCode);
    void slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode);
    void slotIconDownloaded(QByteArray iconData);
    void slotAllowDesktopNotificationsChanged(bool isAllowed);

private:
    QPointer<JsonApiJob> _notificationJob;
    AccountState *_accountState;
};
}

#endif // NOTIFICATIONHANDLER_H