//
//  main.m
//  desktopclient
//
//  SPDX-FileCopyrightText: 2015 ownCloud GmbH
//  SPDX-License-Identifier: GPL-2.0-or-later
//

// This is fake application bundle with the same bundle ID as the real desktop client.
// Xcode needs a wrapping application to allow the extension to be debugged.

#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
    return NSApplicationMain(argc, argv);
}
