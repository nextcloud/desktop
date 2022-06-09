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

#include "3rdparty/QProgressIndicator/QProgressIndicator.h"
#include "pages/abstractsetupwizardpage.h"
#include "pagination.h"
#include "setupwizardaccountbuilder.h"

#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QList>
#include <QPair>
#include <QStackedLayout>
#include <QStackedWidget>

namespace Ui {
class SetupWizardWindow;
}

namespace OCC::Wizard {
/**
     * This class contains the UI-specific code. It hides the complexity from the controller, and provides a high-level API.
     */
class SetupWizardWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SetupWizardWindow(QWidget *parent);
    ~SetupWizardWindow() noexcept override;

    /**
     * Set entries in the pagination at the bottom of the wizard UI.
     * The entries are identified by their position in the list (read: index).
     */
    void setPaginationEntries(const QStringList &paginationEntries);

    /**
     * Render this page within the wizard window.
     * @param page page to render
     * @param index index to highlight in pagination (also used to decide which buttons to enable)
     */
    void displayPage(AbstractSetupWizardPage *page, PageIndex index);

    void showErrorMessage(const QString &errorMessage);

    void disableNextButton();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

Q_SIGNALS:
    void paginationEntryClicked(PageIndex currentPage, PageIndex clickedPageIndex);
    void nextButtonClicked(PageIndex currentPage);
    void backButtonClicked(PageIndex currentPage);

public Q_SLOTS:
    /**
     * Show "transition to next page" animation. Use displayPage(...) to end it.
     */
    void slotStartTransition();

private Q_SLOTS:
    void slotReplaceContent(QWidget *newWidget);
    void slotHideErrorMessageWidget();
    void slotMoveToNextPage();
    void slotUpdateNextButton();

private:
    void loadStylesheet();

    ::Ui::SetupWizardWindow *_ui;

    // the wizard window keeps at most one widget inside the content widget's layout
    // we keep a pointer in order to be able to delete it (and thus remove it from the window) when replacing the content
    QWidget *_currentContentWidget = nullptr;

    // need to keep track of the current page for event filtering
    AbstractSetupWizardPage *_currentPage = nullptr;
    // during a transition, the event filter must be disabled
    bool _transitioning;
};
}
