/*
 * Copyright (C) by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#ifndef TERMSOFSERVICEWIZARDPAGE_H
#define TERMSOFSERVICEWIZARDPAGE_H

#include <QWizardPage>

class QVBoxLayout;

namespace OCC {

class OwncloudWizard;
class TermsOfServiceChecker;

class TermsOfServiceWizardPage : public QWizardPage
{
    Q_OBJECT
public:
    TermsOfServiceWizardPage();

    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] int nextId() const override;
    [[nodiscard]] bool isComplete() const override;

Q_SIGNALS:
    void connectToOCUrl(const QString &);
    void pollNow();

private Q_SLOTS:
    void slotPollNow();
    void termsOfServiceChecked();

private:
    QVBoxLayout *_layout = nullptr;
    OwncloudWizard *_ocWizard = nullptr;
    TermsOfServiceChecker *_termsOfServiceChecker = nullptr;
};

} // namespace OCC

#endif // TERMSOFSERVICEWIZARDPAGE_H
