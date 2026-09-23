/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "capturescene.h"
#include <QWidget>

namespace OCC::DocAssets
{
CaptureScene::CaptureScene() = default;

CaptureScene::~CaptureScene()
{
    widget.reset();
    while (!children().isEmpty()) {
        delete children().constFirst();
    }
}
}
