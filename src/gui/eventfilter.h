#ifndef EVENTFILTER_H
#define EVENTFILTER_H

#include "libsync/theme.h"

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QObject>
#include <QString>

namespace OCC {

	class NCEventFilter : public QAbstractNativeEventFilter
	{
	public:
		virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *) Q_DECL_OVERRIDE;
	};

}
#endif // EVENTFILTER_H
