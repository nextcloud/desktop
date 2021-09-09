#include "datetimeprovider.h"

namespace OCC {

DateTimeProvider::~DateTimeProvider() = default;

QDateTime DateTimeProvider::currentDateTime() const
{
    return QDateTime::currentDateTime();
}

QDate DateTimeProvider::currentDate() const
{
    return QDate::currentDate();
}

}
