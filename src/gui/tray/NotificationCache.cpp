#include "NotificationCache.h"

namespace OCC {

bool NotificationCache::contains(const Notification &notification) const
{
    return _notifications.find(calculateKey(notification)) != _notifications.end();
}

void NotificationCache::insert(const Notification &notification)
{
    _notifications.insert(calculateKey(notification));
}

void NotificationCache::clear()
{
    _notifications.clear();
}

uint NotificationCache::calculateKey(const Notification &notification) const
{
    return qHash(notification.title + notification.message);
}
}
