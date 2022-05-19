#include "setupwizardcontroller.h"

#include "accessmanager.h"
#include "creds/oauth.h"
#include "determineauthtypejobfactory.h"
#include "gui/application.h"
#include "gui/folderman.h"
#include "jobs/checkbasicauthjobfactory.h"
#include "jobs/resolveurljobfactory.h"
#include "pages/accountconfiguredwizardpage.h"
#include "pages/basiccredentialssetupwizardpage.h"
#include "pages/oauthcredentialssetupwizardpage.h"
#include "pages/serverurlsetupwizardpage.h"
#include "theme.h"

#include <QClipboard>
#include <QDir>
#include <QTimer>

using namespace std::chrono_literals;

namespace {

const QString defaultUrlSchemeC = QStringLiteral("https://");
const QStringList supportedUrlSchemesC({ defaultUrlSchemeC, QStringLiteral("http://") });

}

namespace OCC::Wizard {

SetupWizardController::SetupWizardController(QWidget *parent)
    : QObject(parent)
    , _wizardWindow(new SetupWizardWindow(parent))
    , _accessManager(new AccessManager(this))
{
    // initialize pagination
    const QStringList paginationEntries = { tr("Server URL"), tr("Credentials"), tr("Sync Options") };
    _wizardWindow->setPaginationEntries(paginationEntries);

    nextStep(std::nullopt, std::nullopt);

    // allow settings dialog to clean up the wizard controller and all the objects it created
    connect(_wizardWindow, &SetupWizardWindow::rejected, this, [this]() {
        Q_EMIT finished(nullptr, SyncMode::Invalid);
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
            // we don't want to store any unnecessary certificates for this account when the user returns to the first page
            // therefore we clear the certificates storage before resolving the URL
            _accountBuilder = {};

            const auto *pagePtr = qobject_cast<ServerUrlSetupWizardPage *>(_currentPage);

            const auto serverUrl = [pagePtr]() {
                QString userProvidedUrl = pagePtr->userProvidedUrl();

                // fix scheme if necessary
                // using HTTPS as a default is a really good idea nowadays, users can still enter http:// explicitly if they wish to
                if (!std::any_of(supportedUrlSchemesC.begin(), supportedUrlSchemesC.end(), [userProvidedUrl](const QString &scheme) {
                        return userProvidedUrl.startsWith(scheme);
                    })) {
                    qDebug() << "no URL scheme provided, prepending default URL scheme" << defaultUrlSchemeC;
                    userProvidedUrl.prepend(defaultUrlSchemeC);
                }

                return QUrl::fromUserInput(userProvidedUrl);
            }();

            // TODO: perform some better validation
            if (serverUrl.isValid()) {
                // (ab)using account builder as a temporary storage for the server URL
                // below we will set both the resolved URL as well as the actual auth type
                _accountBuilder.setServerUrl(serverUrl, DetermineAuthTypeJob::AuthType::Unknown);
                desiredPage = currentPage.value() + 1;
            } else {
                _wizardWindow->showErrorMessage(tr("Invalid server URL"));
                desiredPage = currentPage.value();
            }
        }

        if (currentPage == 1) {
            if (_accountBuilder.authType() == DetermineAuthTypeJob::AuthType::Basic) {
                const auto *pagePtr = qobject_cast<BasicCredentialsSetupWizardPage *>(_currentPage);

                const auto username = pagePtr->username();
                const auto password = pagePtr->password();

                _accountBuilder.setAuthenticationStrategy(new HttpBasicAuthenticationStrategy(username, password));
            }
            if (_accountBuilder.hasValidCredentials()) {
                desiredPage = currentPage.value() + 1;
            } else {
                _wizardWindow->showErrorMessage(tr("Invalid credentials"));
                desiredPage = currentPage.value();
            }
        }

        // final step
        if (currentPage == 2) {
            const auto *pagePtr = qobject_cast<AccountConfiguredWizardPage *>(_currentPage);

            auto account = _accountBuilder.build();
            Q_ASSERT(account != nullptr);

            QString targetDir = [pagePtr, account]() -> QString {
                if (pagePtr->syncMode() == Wizard::SyncMode::ConfigureUsingFolderWizard) {
                    return {};
                }
                return QDir::fromNativeSeparators(pagePtr->syncTargetDir());
            }();
            account->setDefaultSyncRoot(targetDir);

            Q_EMIT finished(account, pagePtr->syncMode());
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
        auto *messageBox = new QMessageBox(
            QMessageBox::Warning,
            tr("Insecure connection"),
            tr("The connection to %1 is insecure.\nAre you sure you want to proceed?").arg(_accountBuilder.serverUrl().toString()),
            QMessageBox::NoButton,
            _wizardWindow);

        messageBox->setAttribute(Qt::WA_DeleteOnClose);

        messageBox->addButton(QMessageBox::Cancel);
        messageBox->addButton(tr("Confirm"), QMessageBox::YesRole);

        connect(messageBox, &QMessageBox::rejected, this, [showFirstPage]() {
            showFirstPage(tr("Insecure server rejected by user"));
        });

        connect(messageBox, &QMessageBox::accepted, this, [this, showFirstPage]() {
            // when moving back to this page (or retrying a failed credentials check), we need to make sure existing cookies
            // and certificates are deleted from the access manager
            _accessManager->deleteLater();
            _accessManager = new AccessManager(this);

            // first, we must resolve the actual server URL
            auto resolveJob = Jobs::ResolveUrlJobFactory(_accessManager).startJob(_accountBuilder.serverUrl());

            connect(resolveJob, &CoreJob::finished, this, [this, resolveJob, showFirstPage]() {
                resolveJob->deleteLater();

                if (!resolveJob->success()) {
                    // resolving failed, we need to show an error message
                    showFirstPage(resolveJob->errorMessage());
                    return;
                }

                const auto resolvedUrl = qvariant_cast<QUrl>(resolveJob->result());

                // next, we need to find out which kind of authentication page we have to present to the user
                auto authTypeJob = DetermineAuthTypeJobFactory(_accessManager).startJob(resolvedUrl);

                connect(authTypeJob, &CoreJob::finished, authTypeJob, [this, authTypeJob, resolvedUrl]() {
                    authTypeJob->deleteLater();

                    if (authTypeJob->result().isNull()) {
                        _wizardWindow->showErrorMessage(authTypeJob->errorMessage());
                        nextStep(0, 0);
                        return;
                    }

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
                        auto oAuth = new OAuth(_accountBuilder.serverUrl(), QString(), _accessManager, {}, this);

                        connect(oAuth, &OAuth::result, this, [this, newPage](OAuth::Result result, const QString &user, const QString &token, const QString &refreshToken) {
                            // the button may not be clicked any more, since the server has been shut down right before this signal was emitted by the OAuth instance
                            newPage->disableButtons();

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

                        connect(newPage, &OAuthCredentialsSetupWizardPage::openBrowserButtonPushed, this, [oAuth]() {
                            oAuth->openBrowser();
                        });

                        connect(newPage, &OAuthCredentialsSetupWizardPage::copyUrlToClipboardButtonPushed, this, [oAuth]() {
                            // TODO: use authorisationLinkAsync
                            auto link = oAuth->authorisationLink().toString();
                            ocApp()->clipboard()->setText(link);
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

            connect(
                resolveJob, &CoreJob::caCertificateAccepted, this, [this](const QSslCertificate &caCertificate) {
                    // future requests made through this access manager should accept the certificate
                    _accessManager->addCustomTrustedCaCertificates({ caCertificate });


                    // the account maintains a list, too, which is also saved in the config file
                    _accountBuilder.addCustomTrustedCaCertificate(caCertificate);
                },
                Qt::DirectConnection);
        });

        // instead of defining a lambda that we could call from here as well as the message box, we can put the
        // handler into the accepted() signal handler, and emit that signal here
        if (_accountBuilder.serverUrl().scheme() == QStringLiteral("https")) {
            Q_EMIT messageBox->accepted();
        } else {
            messageBox->show();
        }

        return;
    }

    if (desiredPage == 2) {
        auto moveToFinalPage = [this]() {
            // being pessimistic by default
            bool vfsIsAvailable = false;
            bool enableVfsByDefault = false;
            bool vfsModeIsExperimental = false;

            switch (bestAvailableVfsMode()) {
            case Vfs::WindowsCfApi:
                vfsIsAvailable = true;
                enableVfsByDefault = true;
                vfsModeIsExperimental = false;
                break;
            case Vfs::WithSuffix:
                vfsIsAvailable = true;
                enableVfsByDefault = false;
                vfsModeIsExperimental = true;
                break;
            default:
                break;
            }

            _currentPage = new AccountConfiguredWizardPage(FolderMan::suggestSyncFolder(_accountBuilder.serverUrl(), _accountBuilder.displayName()), vfsIsAvailable, enableVfsByDefault, vfsModeIsExperimental);
            _wizardWindow->displayPage(_currentPage, 2);
        };

        if (_accountBuilder.authType() == DetermineAuthTypeJob::AuthType::Basic) {
            auto strategy = dynamic_cast<HttpBasicAuthenticationStrategy *>(_accountBuilder.authenticationStrategy());
            Q_ASSERT(strategy != nullptr);

            auto checkBasicAuthJob = Jobs::CheckBasicAuthJobFactory(_accessManager, strategy->username(), strategy->password(), this).startJob(_accountBuilder.serverUrl());

            auto showCredentialsPageAgain = [this, checkBasicAuthJob](const QString &error) {
                checkBasicAuthJob->deleteLater();

                if (!error.isEmpty()) {
                    _wizardWindow->showErrorMessage(error);
                }

                _currentPage = new BasicCredentialsSetupWizardPage(_accountBuilder.serverUrl());
                _wizardWindow->displayPage(_currentPage, 1);
            };

            connect(checkBasicAuthJob, &CoreJob::finished, this, [moveToFinalPage, checkBasicAuthJob, showCredentialsPageAgain]() {
                if (checkBasicAuthJob->success()) {
                    if (checkBasicAuthJob->result().toBool()) {
                        moveToFinalPage();
                    } else {
                        showCredentialsPageAgain(tr("Login failed: username and/or password incorrect"));
                    }
                } else {
                    showCredentialsPageAgain(tr("Login failed: %1").arg(checkBasicAuthJob->errorMessage()));
                }
            });

            return;
        } else {
            // for all other possible auth types (at the moment, just OAuth2), we do not need to check the credentials, we can reasonably assume they're correct
            moveToFinalPage();
        }

        return;
    }

    Q_UNREACHABLE();
}

SetupWizardController::~SetupWizardController() noexcept
{
    delete _wizardWindow;
    delete _accessManager;
}
}
