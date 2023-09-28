/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "fileprovideritemmetadata.h"

#import <Foundation/Foundation.h>
#import <FileProvider/NSFileProviderItem.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {

QString nsNameComponentsToLocalisedQString(NSPersonNameComponents *const nameComponents)
{
    if (nameComponents == nil) {
        return {};
    }

    NSString *const name = [NSPersonNameComponentsFormatter localizedStringFromPersonNameComponents:nameComponents style:NSPersonNameComponentsFormatterStyleDefault options:0];
    return QString::fromNSString(name);
}

QHash<QString, QByteArray> extendedAttributesToHash(NSDictionary<NSString *, NSData *> *const extendedAttributes)
{
    QHash<QString, QByteArray> hash;
    for (NSString *const key in extendedAttributes) {
        NSData *const value = [extendedAttributes objectForKey:key];
        hash.insert(QString::fromNSString(key), QByteArray::fromNSData(value));
    }
    return hash;
}

}
