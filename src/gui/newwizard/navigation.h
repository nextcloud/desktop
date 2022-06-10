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

#include <QHBoxLayout>
#include <QMap>
#include <QRadioButton>
#include <QString>
#include <QWidget>

namespace OCC::Wizard {

/**
 * Provides a radio button based quick navigation on the wizard's bottom side.
 */
class Navigation : public QWidget
{
    Q_OBJECT

public:
    explicit Navigation(QWidget *parent = nullptr);

    ~Navigation() noexcept override;

    /**
     * Set or replace entries in the navigation.
     * This method creates the corresponding buttons.
     * @param newEntries ordered list of wizard states to be rendered in the navigation
     */
    void setEntries(const QList<SetupWizardState> &newEntries);

Q_SIGNALS:
    /**
     * Emitted when a pagination entry is clicked.
     * This event is only emitted for previous states.
     * @param clickedState state the user wants to switch to
     */
    void paginationEntryClicked(SetupWizardState clickedState);

public Q_SLOTS:
    /**
     * Change to another state. Applies changes to hosted UI elements (e.g., disables buttons, )
     * @param newState state to activate
     */
    void setActiveState(SetupWizardState newState);

private:
    void removeAllItems();
    void enableOrDisableButtons();

    QMap<SetupWizardState, QRadioButton *> _entries;
    SetupWizardState _activeState = SetupWizardState::FirstState;
    bool _enabled = true;
};

}
