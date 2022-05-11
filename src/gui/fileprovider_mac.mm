/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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


#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

#include "application.h"
#include "fileprovider.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProvider, "nextcloud.gui.macfileprovider")

namespace Mac {

class FileProviderInitializer::Private {
  public:
    Private() {
        domainIdentifier = @SOCKETAPI_TEAM_IDENTIFIER_PREFIX APPLICATION_REV_DOMAIN;
        name = @APPLICATION_NAME;
        fileProviderDomain = [[NSFileProviderDomain alloc] initWithIdentifier:domainIdentifier displayName:name];
        setupFileProvider();
    }

    ~Private() = default;

    void setupFileProvider() {
        [NSFileProviderManager addDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                NSLog(@"Add file provider domain: %i %@", [error code], [error localizedDescription]);
            }
        }];
    }

    void removeFileProvider() {
        [NSFileProviderManager removeDomain:fileProviderDomain completionHandler:^(NSError *error) {
            if(error) {
                NSLog(@"Remove file provider domain: %i %@", [error code], [error localizedDescription]);
            }
        }];
    }

    NSFileProviderDomainIdentifier domainIdentifier;
    NSString *name;
    NSFileProviderDomain *fileProviderDomain;
};

FileProviderInitializer::FileProviderInitializer() {
    d = new FileProviderInitializer::Private();
    d->setupFileProvider();
}

FileProviderInitializer::~FileProviderInitializer() {
    d->removeFileProvider();
    delete d;
}

} // namespace Mac
} // namespace OCC
