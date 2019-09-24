#include "eventfilter.h"

#include "../common/utility.h"
#include "../libsync/configfile.h"
#include "theme.h"

#include <QDebug>

#include "windows.h"

namespace OCC {

static inline void processPlatformEventMsg(void *message)
{
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_SETTINGCHANGE) {
        qDebug() << "WM_SETTINGCHANGE event received. Checking for changes in platform-specific theme setting now.";
        if ( ConfigFile().monoIcons() && (Theme::instance()->currentIconFlavor != (Utility::hasDarkSystray() ? Theme::IconFlavor::Light : Theme::IconFlavor::Dark )) ) {
            qDebug() << "Detected inconsistent OS/app systray theme setting. Changing systray icon.";
            Theme::instance()->setOSThemeChanged();
        }
    }
    return;
}

} //OCC