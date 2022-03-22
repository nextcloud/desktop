#include "setupwizardcontroller.h"
#include "creds/oauth.h"
#include "determineauthtypejobfactory.h"
#include "jobs/resolveurljobfactory.h"
#include "pages/accountconfiguredwizardpage.h"
#include "pages/basiccredentialssetupwizardpage.h"
#include "pages/oauthcredentialssetupwizardpage.h"
#include "pages/serverurlsetupwizardpage.h"

#include <QTimer>

#include <chrono>

using namespace std::chrono_literals;

namespace OCC::Wizard {

SetupWizardController::SetupWizardController(QWidget *parent)
    : QObject(parent)
    , _wizardWindow(new SetupWizardWindow(parent))
    , _networkAccessManager(new QNetworkAccessManager(this))
{
    // initialize pagination
    const QStringList paginationEntries = { tr("Server URL"), tr("Credentials"), tr("Sync Options") };
    _wizardWindow->setPaginationEntries(paginationEntries);

    nextStep(std::nullopt, std::nullopt);

    // allow settings dialog to clean up the wizard controller and all the objects it created
    connect(_wizardWindow, &SetupWizardWindow::rejected, this, [this]() {
        Q_EMIT finished(nullptr);
    });

    connect(_wizardWindow, &SetupWizardWindow::paginationEntryClicked, this, [this, paginationEntries](PageIndex currentPage, PageIndex clickedPageIndex) {
        Q_ASSERT(currentPage < paginationEntries.size());

        nextStep(currentPage, clickedPageIndex);
    });
    connect(_wizardWindow, &SetupWizardWindow::nextButtonClicked, this, [this, paginationEntries](PageIndex currentPage) {
        Q_ASSERT(currentPage < paginationEntries.size());

        nextStep(currentPage, std::nullopt);
    });

    // in case the back button is clicked, the current page's data is dismissed, and the previous page should be shown
    connect(_wizardWindow, &SetupWizardWindow::backButtonClicked, this, [this](PageIndex currentPage) {
        // back button should be disabled on the first page
        Q_ASSERT(currentPage > 0);

        nextStep(currentPage, currentPage - 1);
    });
}

SetupWizardWindow *SetupWizardController::window()
{
    return _wizardWindow;
}

void SetupWizardController::nextStep(std::optional<PageIndex> currentPage, std::optional<PageIndex> desiredPage)
{
    // should take care of cleaning up the page once the function has finished
    QScopedPointer<AbstractSetupWizardPage> page(_currentPage);

    // initial state
    if (!currentPage.has_value()) {
        desiredPage = 0;
    }

    // "next button" workflow
    if (!desiredPage.has_value()) {
        // try to fill in data appropriately
        // if it works, go to next page
        // otherwise, show current page again
        if (currentPage == 0) {
            const auto *pagePtr = qobject_cast<ServerUrlSetupWizardPage *>(_currentPage);

            auto serverUrl = pagePtr->serverUrl();

            // fix scheme if necessary
            // the second half is needed for URLs which contain a port (e.g., host:1234), QUrl then parses host: as scheme
            if (serverUrl.scheme().isEmpty()) {
                serverUrl = QUrl(QStringLiteral("https://") + serverUrl.toString());
            }

            // TODO: perform some better validation
            if (serverUrl.isValid()) {
                // (ab)using account builder as a temporary storage for the server URL
                // below we will set both the resolved URL as well as the actual auth type
                _accountBuilder.setServerUrl(serverUrl, DetermineAuthTypeJob::AuthType::Unknown);
                desiredPage = currentPage.value() + 1;
            } else {
                _wizardWindow->showErrorMessage(QStringLiteral("Invalid server URL"));
                desiredPage = currentPage.value();
            }
        }

        if (currentPage == 1) {
            if (_accountBuilder.authType() == DetermineAuthTypeJob::AuthType::Basic) {
                const auto *pagePtr = qobject_cast<BasicCredentialsSetupWizardPage *>(_currentPage);

                const auto username = pagePtr->username();
                const auto password = pagePtr->password();

                _accountBuilder.setAuthenticationStrategy(new HttpBasicAuthenticationStrategy(username, password));

                // TODO: actually check whether creds are correct
                if (_accountBuilder.hasValidCredentials()) {
                    desiredPage = currentPage.value() + 1;
                } else {
                    _wizardWindow->showErrorMessage(QStringLiteral("Invalid credentials"));
                    desiredPage = currentPage.value();
                }
            }

            if (_accountBuilder.authType() == DetermineAuthTypeJob::AuthType::OAuth) {
                // authentication data is filled in asynchronously, hence all we have to do here is determine the next page
                if (_accountBuilder.hasValidCredentials()) {
                    desiredPage = currentPage.value() + 1;
                } else {
                    _wizardWindow->showErrorMessage(QStringLiteral("Invalid credentials"));
                    desiredPage = currentPage.value();
                }
            }
        }

        // final step
        if (currentPage == 2) {
            auto account = _accountBuilder.build();
            Q_ASSERT(account != nullptr);
            emit finished(account);
            return;
        }
    }

    auto showFirstPage = [this](const QString &error = QString()) {
        if (!error.isEmpty()) {
            _wizardWindow->showErrorMessage(error);
        }

        _currentPage = new ServerUrlSetupWizardPage(_accountBuilder.serverUrl());
        _wizardWindow->displayPage(_currentPage, 0);
    };

    if (desiredPage == 0) {
        showFirstPage();
        return;
    }

    if (desiredPage == 1) {
        // first, we must resolve the actual server URL
        auto resolveJob = Jobs::ResolveUrlJobFactory(_networkAccessManager).startJob(_accountBuilder.serverUrl());

        connect(resolveJob, &CoreJob::finished, this, [this, resolveJob, showFirstPage]() {
            resolveJob->deleteLater();

            if (!resolveJob->success()) {
                // resolving failed, we need to show an error message
                showFirstPage(resolveJob->errorMessage());
                return;
            }

            const auto resolvedUrl = qvariant_cast<QUrl>(resolveJob->result());

            // next, we need to find out which kind of authentication page we have to present to the user
            auto authTypeJob = DetermineAuthTypeJobFactory(_networkAccessManager).startJob(resolvedUrl);

            connect(authTypeJob, &CoreJob::finished, authTypeJob, [this, authTypeJob, resolvedUrl]() {
                authTypeJob->deleteLater();

                _accountBuilder.setServerUrl(resolvedUrl, qvariant_cast<DetermineAuthTypeJob::AuthType>(authTypeJob->result()));

                switch (_accountBuilder.authType()) {
                case DetermineAuthTypeJob::AuthType::Basic: {
                    _currentPage = new BasicCredentialsSetupWizardPage(_accountBuilder.serverUrl());
                    _wizardWindow->displayPage(_currentPage, 1);
                    return;
                }

                case DetermineAuthTypeJob::AuthType::OAuth: {
                    auto newPage = new OAuthCredentialsSetupWizardPage(_accountBuilder.serverUrl());

                    // username might not be set yet, shouldn't matter, though
                    auto oAuth = new OAuth(_accountBuilder.serverUrl(), QStringLiteral(), _networkAccessManager, {}, this);

                    connect(oAuth, &OAuth::result, this, [this, newPage](OAuth::Result result, const QString &user, const QString &token, const QString &refreshToken) {
                        // the button may not be clicked any more, since the server has been shut down right before this signal was emitted by the OAuth instance
                        newPage->disableReopenBrowserButton();

                        _wizardWindow->slotStartTransition();

                        // bring window up top again, as the browser may have been raised in front of it
                        _wizardWindow->raise();

                        switch (result) {
                        case OAuth::Result::LoggedIn: {
                            _accountBuilder.setAuthenticationStrategy(new OAuth2AuthenticationStrategy(user, token, refreshToken));
                            nextStep(1, std::nullopt);
                            break;
                        }
                        case OAuth::Result::Error: {
                            _wizardWindow->showErrorMessage(tr("Error while trying to log into OAuth2-enabled server."));
                            nextStep(1, 0);
                            break;
                        }
                        case OAuth::Result::NotSupported: {
                            // should never happen
                            _wizardWindow->showErrorMessage(tr("Server reports that OAuth2 is not supported."));
                            nextStep(1, 0);
                            break;
                        }
                        }
                    });

                    connect(newPage, &OAuthCredentialsSetupWizardPage::reopenBrowserButtonPushed, this, [oAuth]() {
                        oAuth->openBrowser();
                    });

                    _currentPage = newPage;
                    _wizardWindow->displayPage(_currentPage, 1);

                    // moving to next page is only possible once we see a request to our embedded web server
                    _wizardWindow->disableNextButton();

                    oAuth->startAuthentication();

                    return;
                };

                default:
                    Q_UNREACHABLE();
                }
            });
        });

        return;
    }

    if (desiredPage == 2) {
        _currentPage = new AccountConfiguredWizardPage;
        _wizardWindow->displayPage(_currentPage, 2);
        return;
    }

    Q_UNREACHABLE();
}

SetupWizardController::~SetupWizardController() noexcept
{
    delete _wizardWindow;
    delete _networkAccessManager;
}
}
