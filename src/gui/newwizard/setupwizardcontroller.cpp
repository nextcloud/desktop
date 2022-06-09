#include "setupwizardcontroller.h"

#include "accessmanager.h"
#include "determineauthtypejobfactory.h"
#include "gui/application.h"
#include "gui/folderman.h"
#include "pages/accountconfiguredwizardpage.h"
#include "pages/basiccredentialssetupwizardpage.h"
#include "states/abstractsetupwizardstate.h"
#include "states/accountconfiguredsetupwizardstate.h"
#include "states/basiccredentialssetupwizardstate.h"
#include "states/oauthcredentialssetupwizardstate.h"
#include "states/serverurlsetupwizardstate.h"

#include <QClipboard>
#include <QTimer>

using namespace std::chrono_literals;

namespace OCC::Wizard {

Q_LOGGING_CATEGORY(lcSetupWizardController, "setupwizard.controller")

SetupWizardController::SetupWizardController(QWidget *parent)
    : QObject(parent)
    , _context(new SetupWizardContext(parent, this))
{
    // initialize pagination
    const QStringList paginationEntries = { tr("Server URL"), tr("Credentials"), tr("Sync Options") };
    _context->window()->setPaginationEntries(paginationEntries);

    nextStep(std::nullopt);

    // allow settings dialog to clean up the wizard controller and all the objects it created
    connect(_context->window(), &SetupWizardWindow::rejected, this, [this]() {
        qCDebug(lcSetupWizardController) << "wizard window closed";
        Q_EMIT finished(nullptr, SyncMode::Invalid);
    });

    connect(_context->window(), &SetupWizardWindow::paginationEntryClicked, this, [this, paginationEntries](PageIndex currentPage, PageIndex clickedPageIndex) {
        Q_ASSERT(currentPage < paginationEntries.size());
        qCDebug(lcSetupWizardController) << "pagination entry clicked: current page" << currentPage << "clicked page" << clickedPageIndex;
        nextStep(clickedPageIndex);
    });

    connect(_context->window(), &SetupWizardWindow::nextButtonClicked, this, [this, paginationEntries](PageIndex currentPage) {
        Q_ASSERT(currentPage < paginationEntries.size());
        qCDebug(lcSetupWizardController) << "next button clicked on current page" << currentPage;
        _currentState->evaluatePage();
    });

    // in case the back button is clicked, the current page's data is dismissed, and the previous page should be shown
    connect(_context->window(), &SetupWizardWindow::backButtonClicked, this, [this](PageIndex currentPage) {
        // back button should be disabled on the first page
        Q_ASSERT(currentPage > 0);
        qCDebug(lcSetupWizardController) << "back button clicked on current page" << currentPage;
        nextStep(currentPage - 1);
    });
}

SetupWizardWindow *SetupWizardController::window()
{
    return _context->window();
}

void SetupWizardController::nextStep(std::optional<PageIndex> desiredPage)
{
    // should take care of cleaning up the page once the function has finished
    QScopedPointer<AbstractSetupWizardState> page(_currentState);

    // initial state
    if (_currentState == nullptr) {
        Q_ASSERT(!desiredPage.has_value());
        desiredPage = 0;
    }

    // "next button" workflow
    if (!desiredPage.has_value()) {
        switch (_currentState->state()) {
        case SetupWizardState::ServerUrlState: {
            desiredPage = 1;
            break;
        }
        case SetupWizardState::CredentialsState: {
            desiredPage = 2;
            break;
        }
        case SetupWizardState::AccountConfiguredState: {
            const auto *pagePtr = qobject_cast<AccountConfiguredWizardPage *>(_currentState->page());

            auto account = _context->accountBuilder().build();
            Q_ASSERT(account != nullptr);
            Q_EMIT finished(account, pagePtr->syncMode());
            return;
        }
        default:
            Q_UNREACHABLE();
        }
    }

    if (_currentState != nullptr) {
        _currentState->deleteLater();
    }

    switch (desiredPage.value()) {
    case 0: {
        _currentState = new ServerUrlSetupWizardState(_context);
        break;
    }
    case 1: {
        switch (_context->accountBuilder().authType()) {
        case DetermineAuthTypeJob::AuthType::Basic:
            _currentState = new BasicCredentialsSetupWizardState(_context);
            break;
        case DetermineAuthTypeJob::AuthType::OAuth:
            _currentState = new OAuthCredentialsSetupWizardState(_context);
            break;
        default:
            Q_UNREACHABLE();
        }

        break;
    }
    case 2: {
        _currentState = new AccountConfiguredSetupWizardState(_context);
        break;
    }
    default:
        Q_UNREACHABLE();
    }

    OC_ASSERT(desiredPage.has_value());
    OC_ASSERT(_currentState != nullptr);

    qDebug() << "Current wizard state:" << _currentState->state();

    connect(_currentState, &AbstractSetupWizardState::evaluationSuccessful, this, [this, desiredPage]() {
        _currentState->deleteLater();
        //        nextStep(desiredPage.value() + 1);
        nextStep(std::nullopt);
    });

    connect(_currentState, &AbstractSetupWizardState::evaluationFailed, this, [this, desiredPage](const QString &errorMessage) {
        _currentState->deleteLater();
        _context->window()->showErrorMessage(errorMessage);
        nextStep(desiredPage);
    });

    _context->window()->displayPage(_currentState->page(), desiredPage.value());
}

SetupWizardController::~SetupWizardController() noexcept
{
    _context->deleteLater();
}
}
