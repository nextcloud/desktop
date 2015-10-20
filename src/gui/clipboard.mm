#include <QString>
#include <QDebug>
#import <Cocoa/Cocoa.h>

namespace OCC {

// https://github.com/owncloud/client/issues/3300
void copyToPasteboard(const QString &string)
{
    qDebug() << Q_FUNC_INFO << string;
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] setString:[NSString stringWithUTF8String:string.toUtf8().data()]
                                      forType:NSStringPboardType];
}

}
