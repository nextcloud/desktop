/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PROVIDERSIGNUPPAGE_H
#define PROVIDERSIGNUPPAGE_H

#include <QWizardPage>

class QVBoxLayout;

namespace OCC {

class Flow2AuthWidget;

class ProviderSignupPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ProviderSignupPage(QWidget *parent = nullptr);

    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] bool isComplete() const override;

public slots:
    void slotStyleChanged();

private:
    QVBoxLayout *_layout = nullptr;
    Flow2AuthWidget *_flow2AuthWidget = nullptr;
};

} // namespace OCC

#endif // PROVIDERSIGNUPPAGE_H
