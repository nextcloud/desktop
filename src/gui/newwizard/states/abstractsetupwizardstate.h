/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#pragma once

#include "enums.h"
#include "pages/abstractsetupwizardpage.h"
#include "setupwizardaccountbuilder.h"
#include "setupwizardcontext.h"
#include "setupwizardwindow.h"

namespace OCC::Wizard {

/**
 * To improve the code structure and increase the readability, we use the state pattern to organize the different wizard states.
 * Like pages, states are "disposable": every transition between pages resp. states (even back to the current state/page) requires
 * creating a new state object. The state object initializes itself from the data available in the account builder.
 *
 * This base class further implements a variant of the abstract factory pattern (pages are created by the constructors and can be
 * fetched through a generic getter).
 */
class AbstractSetupWizardState : public QObject
{
    Q_OBJECT

public:
    /**
     * Used to display page within content widget.
     * @return page associated to this state
     */
    [[nodiscard]] AbstractSetupWizardPage *page() const;

    /**
     * Allows the identification of the underlying state object.
     * @return current state
     */
    [[nodiscard]] virtual SetupWizardState state() const = 0;

    /**
     * Start asynchronous evaluation of the current page. Typically, one or more HTTP requests are sent to the server, and the results are validated.
     * This method is usually called when the user clicks on the "Next" button.
     */
    virtual void evaluatePage() = 0;

Q_SIGNALS:
    /**
     * Emitted when evaluatePage() has completed successfully.
     */
    void evaluationSuccessful();

    /**
     * Emitted when evaluatePage() has found an error.
     */
    void evaluationFailed(QString errorMessage);

protected:
    explicit AbstractSetupWizardState(SetupWizardContext *context);

    SetupWizardContext *_context;

    AbstractSetupWizardPage *_page = nullptr;
};

} // OCC::Wizard
