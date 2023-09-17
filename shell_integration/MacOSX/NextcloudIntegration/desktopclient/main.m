//
//  main.m
//  desktopclient
//
//  Created by Jocelyn Turcotte on 01/06/15.
//
//

// This is fake application bundle with the same bundle ID as the real desktop client.
// Xcode needs a wrapping application to allow the extension to be debugged.

#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
    return NSApplicationMain(argc, argv);
}
