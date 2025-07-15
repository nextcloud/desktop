/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CLIENTMODEWIZARDPAGE_H
#define CLIENTMODEWIZARDPAGE_H

#include "wizard/owncloudwizard.h"

class QVBoxLayout;
class QWidget;
class QQuickView;

namespace OCC {

class ClientModeWizardPage : public QWizardPage
{
    Q_OBJECT
public:
    ClientModeWizardPage();

    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] int nextId() const override;
    [[nodiscard]] bool isComplete() const override;

// public Q_SLOTS:
//     void slotStyleChanged();

// Q_SIGNALS:
//     void styleChanged();

private:
    QVBoxLayout *_layout = nullptr;
    OwncloudWizard *_ocWizard = nullptr;
    QQuickView *_quickView = nullptr;
    QWidget *_windowContainer = nullptr;
};

}

#endif // CLIENTMODEWIZARDPAGE_H
