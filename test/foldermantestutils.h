/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "gui/folderman.h"

using namespace OCC;

/// Helper class to enable usage of FolderMan::instance() from within the
/// tested code.
class FolderManTestHelper : public QObject
{
    Q_OBJECT

public:
    explicit FolderManTestHelper(QObject *parent = nullptr);
    ~FolderManTestHelper() override;

    FolderMan fm;
};
