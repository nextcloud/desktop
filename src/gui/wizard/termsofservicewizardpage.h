/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TERMSOFSERVICEWIZARDPAGE_H
#define TERMSOFSERVICEWIZARDPAGE_H

#include <QWizardPage>

class QVBoxLayout;

namespace OCC {

class OwncloudWizard;
class TermsOfServiceChecker;
class TermsOfServiceCheckWidget;

class TermsOfServiceWizardPage : public QWizardPage
{
    Q_OBJECT
public:
    TermsOfServiceWizardPage();

    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] int nextId() const override;
    [[nodiscard]] bool isComplete() const override;

public Q_SLOTS:
    void slotPollNow();

Q_SIGNALS:
    void pollNow();
    void styleChanged();

private:
    QVBoxLayout *_layout = nullptr;
    OwncloudWizard *_ocWizard = nullptr;
    TermsOfServiceChecker *_termsOfServiceChecker = nullptr;
    TermsOfServiceCheckWidget *_termsOfServiceCheckWidget = nullptr;

private Q_SLOTS:
    void termsOfServiceChecked();
};

} // namespace OCC

#endif // TERMSOFSERVICEWIZARDPAGE_H
