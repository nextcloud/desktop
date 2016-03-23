/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QtCore>

#include "activitydata.h"


namespace OCC
{

bool Activity::operator<( const Activity& val ) const {
    return _dateTime.toMSecsSinceEpoch() > val._dateTime.toMSecsSinceEpoch();
}

bool Activity::operator==( const Activity& val ) const {
    return (_type == val._type && _id == val._id && _accName == val._accName);
}

Activity::Identifier Activity::ident() const {
    return Identifier( _id, _accName );
}


}
