/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_WIZARD_ABSTRACT_CREDS_WIZARD_PAGE_H
#define MIRALL_WIZARD_ABSTRACT_CREDS_WIZARD_PAGE_H

#include <QWizardPage>

namespace OCC {

class AbstractCredentials;

/**
 * @brief The AbstractCredentialsWizardPage class
 * @ingroup gui
 */
class AbstractCredentialsWizardPage : public QWizardPage
{
public:
    void cleanupPage() override;
    [[nodiscard]] int nextId() const override;
    [[nodiscard]] virtual AbstractCredentials *getCredentials() const = 0;
};

} // namespace OCC

#endif
