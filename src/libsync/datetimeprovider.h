#pragma once

#include "owncloudlib.h"

#include <QDateTime>

namespace OCC {

class OWNCLOUDSYNC_EXPORT DateTimeProvider
{
public:
    virtual ~DateTimeProvider();

    [[nodiscard]] virtual QDateTime currentDateTime() const;

    [[nodiscard]] virtual QDate currentDate() const;
};
}
