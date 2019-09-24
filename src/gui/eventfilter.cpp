#include "eventfilter.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)
#include "eventfilter_win.cpp"
#elif defined(Q_OS_MAC)
#include "eventfilter_mac.cpp"
#else
#include "eventfilter_unix.cpp"
#endif

namespace OCC {

// Override nativeEventFilter to actually catch these for forther filtering & processing
bool NCEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *)
    {
		// message processing handed over to platform specific implementations
		processPlatformEventMsg(message);
        return false;
    }

}