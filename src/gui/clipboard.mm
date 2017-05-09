#include <QString>
#import <Cocoa/Cocoa.h>

namespace OCC {

// https://github.com/owncloud/client/issues/3300
void copyToPasteboard(const QString &string)
{
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] setString:[NSString stringWithUTF8String:string.toUtf8().data()]
                                      forType:NSStringPboardType];
}

}
