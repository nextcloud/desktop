#include <QString>
#include <QDebug>
#import <Cocoa/Cocoa.h>

namespace OCC {

bool canOsXSendUserNotification()
{
    return NSClassFromString(@"NSUserNotificationCenter") != nil;
}

void sendOsXUserNotification(const QString &title, const QString &message)
{
    qDebug() << Q_FUNC_INFO << title << message;
    Class cuserNotificationCenter = NSClassFromString(@"NSUserNotificationCenter");
    id userNotificationCenter = [cuserNotificationCenter defaultUserNotificationCenter];

    Class cuserNotification = NSClassFromString(@"NSUserNotification");
    id notification = [[cuserNotification alloc] init];
    [notification setTitle:[NSString stringWithUTF8String:title.toUtf8().data()]];
    [notification setInformativeText:[NSString stringWithUTF8String:message.toUtf8().data()]];

    [userNotificationCenter deliverNotification:notification];
    [notification release];
}

}
