#pragma once

#include <QSet>

namespace OCC {

class NotificationCache
{
public:
    struct Notification
    {
        QString title;
        QString message;
    };

    bool contains(const Notification &notification) const;

    void insert(const Notification &notification);

    void clear();

private:
    uint calculateKey(const Notification &notification) const;


    QSet<uint> _notifications;
};
}
