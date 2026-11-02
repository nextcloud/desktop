/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Sparkle/Sparkle.h>

namespace SparkleUpdaterTestController
{

void install();
void restore();
void failInitialization(bool fail);
id<SPUUpdaterDelegate> capturedDelegate();
SPUStandardUpdaterController *capturedController();

}
