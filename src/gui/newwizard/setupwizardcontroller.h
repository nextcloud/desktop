#pragma once

#include "pages/abstractsetupwizardpage.h"
#include "syncmode.h"
#include <QDialog>
#include <account.h>
#include <optional>
#include <setupwizardaccountbuilder.h>
#include <setupwizardwindow.h>

namespace OCC::Wizard {
/**
 * This class is the backbone of the new setup wizard. It instantiates the required UI elements and fills them with the correct data. It also provides the public API for the settings UI.
 *
 * The new setup wizard uses dependency injection where applicable. The account object is created using the builder pattern.
 */
class SetupWizardController : public QObject
{
    Q_OBJECT

public:
    explicit SetupWizardController(QWidget *parent);
    ~SetupWizardController() noexcept override;

    /**
     * Provides access to the controller's setup wizard window.
     * @return pointer to window
     */
    SetupWizardWindow *window();

Q_SIGNALS:
    /**
     * Emitted when the wizard has finished. It passes the built account object.
     */
    void finished(AccountPtr newAccount, const QString &localFolder, SyncMode syncMode);

private:
    void nextStep(std::optional<PageIndex> currentPage, std::optional<PageIndex> desiredPage);

    SetupWizardWindow *_wizardWindow;

    // keeping a pointer on the current page allows us to check whether the controller has been initialized yet
    // the pointer is also used to clean up the page
    QPointer<AbstractSetupWizardPage> _currentPage;

    SetupWizardAccountBuilder _accountBuilder;

    QNetworkAccessManager *_networkAccessManager;
};
}
