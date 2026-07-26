/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "notificationsoundplayer.h"
#include "notificationsoundplayer_p.h"

#include <QLoggingCategory>
#include <QString>

@interface NCNotificationSoundPlayerDelegate : NSObject <AVAudioPlayerDelegate>
@property (nonatomic, copy) void(^onFinished)(void);
@end

@implementation NCNotificationSoundPlayerDelegate

- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)player successfully:(BOOL)flag
{
    Q_UNUSED(player)
    Q_UNUSED(flag)
    if (self.onFinished) {
        self.onFinished();
    }
}

- (void)dealloc
{
    [_onFinished release];
    [super dealloc];
}

@end

namespace OCC {

Q_LOGGING_CATEGORY(lcNotificationSoundPlayerMac, "nextcloud.gui.notificationsoundplayer.mac", QtInfoMsg)

namespace {

class MacBackend : public NotificationSoundPlayer::Backend
{
public:
    MacBackend()
    {
        @autoreleasepool {
            _delegate = [[NCNotificationSoundPlayerDelegate alloc] init];
            auto *self_ = this;
            _delegate.onFinished = ^{
                if (self_->onFinished) {
                    self_->onFinished();
                }
            };
        }
    }

    ~MacBackend() override
    {
        @autoreleasepool {
            _delegate.onFinished = nil;
            if (_player) {
                [_player stop];
                [_player setDelegate:nil];
                [_player release];
                _player = nil;
            }
            [_delegate release];
            _delegate = nil;
        }
    }

    void setSource(const QString &filesystemPath) override
    {
        @autoreleasepool {
            if (_player) {
                [_player stop];
                [_player setDelegate:nil];
                [_player release];
                _player = nil;
            }

            NSString *path = filesystemPath.toNSString();
            NSURL *url = [NSURL fileURLWithPath:path];
            NSError *error = nil;
            _player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];

            if (!_player) {
                qCWarning(lcNotificationSoundPlayerMac)
                    << "Could not load sound from" << filesystemPath
                    << "-" << QString::fromNSString([error localizedDescription]);
                return;
            }

            [_player setDelegate:_delegate];
            [_player prepareToPlay];
        }
    }

    void play(int loops) override
    {
        @autoreleasepool {
            if (!_player) {
                return;
            }
            [_player setNumberOfLoops:(loops - 1)];
            [_player setCurrentTime:0];
            [_player play];
        }
    }

    void stop() override
    {
        @autoreleasepool {
            if (_player) {
                [_player stop];
            }
        }
    }

    [[nodiscard]] bool handlesLoopsNatively() const override { return true; }
    [[nodiscard]] bool needsFilesystemPath() const override { return true; }

private:
    AVAudioPlayer *_player = nil;
    NCNotificationSoundPlayerDelegate *_delegate = nil;
};

}

std::unique_ptr<NotificationSoundPlayer::Backend> createNotificationSoundPlayerBackend()
{
    return std::make_unique<MacBackend>();
}

}
