#include "setupwizardwindow.h"
#include "ui_setupwizardwindow.h"

#include "gui/application.h"
#include "gui/guiutility.h"
#include "gui/owncloudgui.h"
#include "gui/settingsdialog.h"
#include "theme.h"

#include <QLabel>
#include <QMessageBox>
#include <QStyleFactory>

using namespace std::chrono_literals;

namespace {

using namespace OCC;

QString replaceCssColors(const QString &stylesheet)
{
    return Utility::renderTemplate(stylesheet, {
                                                   { QStringLiteral("WIZARD_BACKGROUND_COLOR"), Theme::instance()->wizardHeaderBackgroundColor().name() }, //
                                                   { QStringLiteral("WIZARD_FONT_COLOR"), Theme::instance()->wizardHeaderTitleColor().name() } //
                                               });
}

}

namespace OCC::Wizard {

Q_LOGGING_CATEGORY(lcSetupWizardWindow, "setupwizard.window")

SetupWizardWindow::SetupWizardWindow(SettingsDialog *parent)
    : QDialog(parent)
    , _ui(new ::Ui::SetupWizardWindow)
{
    Utility::setModal(this);
    setWindowFlag(Qt::WindowCloseButtonHint, false);

    _ui->setupUi(this);

    slotHideErrorMessageWidget();

    // cannot do this in Qt Designer
    _ui->contentWidget->layout()->setAlignment(Qt::AlignCenter);

    connect(_ui->cancelButton, &QPushButton::clicked, this, &SetupWizardWindow::reject);

    connect(_ui->nextButton, &QPushButton::clicked, this, &SetupWizardWindow::slotMoveToNextPage);

    connect(_ui->backButton, &QPushButton::clicked, this, [this]() {
        slotStartTransition();
        Q_EMIT backButtonClicked();
    });

    connect(_ui->navigation, &Navigation::paginationEntryClicked, this, [this](SetupWizardState clickedState) {
        slotStartTransition();
        Q_EMIT navigationEntryClicked(clickedState);
    });

    resize(ocApp()->gui()->settingsDialog()->sizeHintForChild());

    // different styles (e.g., 'Windows', 'Fusion') may require different approaches in the stylesheet
    // therefore we want to force a standard style on all platforms
    // this further makes sure the wizard (well, its contents) looks exactly the same on all platforms
    // Fusion should be available everywhere
    auto fusionStyle = QStyleFactory::create(QStringLiteral("Fusion"));
    if (OC_ENSURE(fusionStyle != nullptr)) {
        _ui->contentWidget->setStyle(fusionStyle);
    } else {
        qCDebug(lcSetupWizardWindow) << "Could not set up default style, wizard contents will be shown using default style";
    }

    loadStylesheet();

    _ui->transitionProgressIndicator->setFixedSize(32, 32);
    _ui->transitionProgressIndicator->setColor(Theme::instance()->wizardHeaderTitleColor());

    // handle user pressing enter/return key
    installEventFilter(this);
}

void SetupWizardWindow::loadStylesheet()
{
    QString path = QStringLiteral(":/client/resources/wizard/style.qss");

    QFile file(path);
    Q_ASSERT(file.exists());
    if (!OC_ENSURE(file.open(QIODevice::ReadOnly))) {
        qCCritical(lcSetupWizardWindow) << "failed to load stylesheet";
    }

    QString stylesheet = replaceCssColors(QString::fromUtf8(file.readAll()));
    _ui->contentWidget->setStyleSheet(stylesheet);
}

void SetupWizardWindow::displayPage(AbstractSetupWizardPage *page, SetupWizardState state)
{
    _transitioning = false;
    _ui->backButton->setEnabled(true);
    _ui->nextButton->setEnabled(true);

    if (state == SetupWizardState::FirstState) {
        _ui->backButton->setEnabled(false);
    } else if (state == SetupWizardState::FinalState) {
        _ui->nextButton->setEnabled(false);
    }

    if (state == SetupWizardState::FinalState) {
        _ui->nextButton->setText(tr("Finish"));
    } else {
        _ui->nextButton->setText(tr("Next >"));
    }

    _currentPage = page;
    slotReplaceContent(_currentPage);

    // initial check whether to enable the next button right away
    slotUpdateNextButton();

    _ui->navigation->setActiveState(state);
    _ui->navigation->setEnabled(true);

    connect(_ui->errorMessageDismissButton, &QPushButton::clicked, this, &SetupWizardWindow::slotHideErrorMessageWidget);

    // by default, set focus on the next button
    _ui->nextButton->setFocus();

    // bring to front if necessary
    ownCloudGui::raiseDialog(this);

    connect(_currentPage, &AbstractSetupWizardPage::contentChanged, this, &SetupWizardWindow::slotUpdateNextButton);

    QTimer::singleShot(0, [this]() {
        // this can optionally be overwritten by the page
        Q_EMIT _currentPage->pageDisplayed();
    });
}

void SetupWizardWindow::slotStartTransition()
{
    _transitioning = true;

    _ui->transitionProgressIndicator->startAnimation();
    _ui->contentWidget->setCurrentWidget(_ui->transitionPage);

    // until a new page is displayed by the controller, we want to prevent the user from initiating another page change
    _ui->backButton->setEnabled(false);
    _ui->nextButton->setEnabled(false);
    _ui->navigation->setEnabled(false);
    // also, we should assume the user has seen the error message in case one is shown
    slotHideErrorMessageWidget();
}

void SetupWizardWindow::reject()
{
    auto messageBox = new QMessageBox(QMessageBox::Warning, tr("Cancel Setup"), tr("Do you really want to cancel the account setup?"), QMessageBox::Yes | QMessageBox::No, ocApp()->gui()->settingsDialog());
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    connect(messageBox, &QMessageBox::accepted, this, [this] {
        // call the base implementation
        QDialog::reject();
    });
    messageBox->show();
    ocApp()->gui()->raiseDialog(messageBox);
}

void SetupWizardWindow::slotReplaceContent(QWidget *newWidget)
{
    _ui->contentWidget->removeWidget(_currentContentWidget);

    _ui->contentWidget->addWidget(newWidget);
    _ui->contentWidget->setCurrentWidget(newWidget);

    _currentContentWidget = newWidget;

    // inheriting the style sheet from content widget doesn't work in all cases
    _currentContentWidget->setStyleSheet(_ui->contentWidget->styleSheet());
}

void SetupWizardWindow::slotHideErrorMessageWidget()
{
    _ui->errorMessageWidget->hide();
}

void SetupWizardWindow::showErrorMessage(const QString &errorMessage)
{
    _ui->errorMessageLabel->setText(errorMessage);
    _ui->errorMessageLabel->setWordWrap(true);
    _ui->errorMessageWidget->show();
}

void SetupWizardWindow::setNavigationEntries(const QList<SetupWizardState> &entries)
{
    _ui->navigation->setEntries(entries);
}

void SetupWizardWindow::slotUpdateNextButton()
{
    _ui->nextButton->setEnabled(_currentPage->validateInput());
}

bool SetupWizardWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!_transitioning) {
        if (obj == _currentPage || obj == this) {
            if (event->type() == QEvent::KeyPress) {
                auto keyEvent = dynamic_cast<QKeyEvent *>(event);

                switch (keyEvent->key()) {
                case Qt::Key_Enter:
                    Q_FALLTHROUGH();
                case Qt::Key_Return:
                    slotMoveToNextPage();
                    return true;
                default:
                    // no action required, give other handlers a chance
                    break;
                }
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void SetupWizardWindow::disableNextButton()
{
    _ui->nextButton->setEnabled(false);
}

SetupWizardWindow::~SetupWizardWindow() noexcept
{
    delete _ui;
}

void SetupWizardWindow::slotMoveToNextPage()
{
    if (OC_ENSURE(_currentPage->validateInput())) {
        slotStartTransition();
        Q_EMIT nextButtonClicked();
    }
}
}
