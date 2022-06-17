#include "setupwizardwindow.h"
#include "ui_setupwizardwindow.h"

#include "gui/application.h"
#include "gui/owncloudgui.h"
#include "gui/settingsdialog.h"
#include "theme.h"

#include <QLabel>
#include <QStyleFactory>

using namespace std::chrono_literals;

namespace {

using namespace OCC;

QString replaceCssColors(QString stylesheet)
{
    QString rv = stylesheet;

    rv = stylesheet.replace(QStringLiteral("@WIZARD_BACKGROUND_COLOR@"), Theme::instance()->wizardHeaderBackgroundColor().name());
    rv = stylesheet.replace(QStringLiteral("@WIZARD_FONT_COLOR@"), Theme::instance()->wizardHeaderTitleColor().name());

    return rv;
}

}

namespace OCC::Wizard {

Q_LOGGING_CATEGORY(lcSetupWizardWindow, "setupwizard.window")

SetupWizardWindow::SetupWizardWindow(QWidget *parent)
    : QDialog(parent)
    , _ui(new ::Ui::SetupWizardWindow)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    _ui->setupUi(this);

    slotHideErrorMessageWidget();

    // cannot do this in Qt Designer
    _ui->contentWidget->layout()->setAlignment(Qt::AlignCenter);

    connect(_ui->cancelButton, &QPushButton::clicked, this, &SetupWizardWindow::reject);

    connect(_ui->nextButton, &QPushButton::clicked, this, &SetupWizardWindow::slotMoveToNextPage);

    connect(_ui->backButton, &QPushButton::clicked, this, [this]() {
        slotStartTransition();
        emit backButtonClicked(_ui->pagination->activePageIndex());
    });

    connect(_ui->pagination, &Pagination::paginationEntryClicked, this, [this](PageIndex clickedPageIndex) {
        slotStartTransition();
        emit paginationEntryClicked(_ui->pagination->activePageIndex(), clickedPageIndex);
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

void SetupWizardWindow::displayPage(AbstractSetupWizardPage *page, PageIndex index)
{
    _transitioning = false;
    _ui->backButton->setEnabled(true);
    _ui->nextButton->setEnabled(true);

    if (index == 0) {
        _ui->backButton->setEnabled(false);
    } else if (index == _ui->pagination->entriesCount()) {
        _ui->nextButton->setEnabled(false);
    }

    if (index >= (_ui->pagination->entriesCount() - 1)) {
        _ui->nextButton->setText(tr("Finish"));
    } else {
        _ui->nextButton->setText(tr("Next >"));
    }

    _currentPage = page;
    slotReplaceContent(_currentPage);

    // initial check whether to enable the next button right away
    slotUpdateNextButton();

    _ui->pagination->setActivePageIndex(index);
    _ui->pagination->setEnabled(true);

    connect(_ui->errorMessageDismissButton, &QPushButton::clicked, this, &SetupWizardWindow::slotHideErrorMessageWidget);

    // by default, set focus on the next button
    _ui->nextButton->setFocus();

    // bring to front if necessary
    ownCloudGui::raiseDialog(this);

    // this can optionally be overwritten by the page
    Q_EMIT _currentPage->pageDisplayed();
}

void SetupWizardWindow::slotStartTransition()
{
    _transitioning = true;

    _ui->transitionProgressIndicator->startAnimation();
    _ui->contentWidget->setCurrentWidget(_ui->transitionPage);

    // until a new page is displayed by the controller, we want to prevent the user from initiating another page change
    _ui->backButton->setEnabled(false);
    _ui->nextButton->setEnabled(false);
    _ui->pagination->setEnabled(false);
    // also, we should assume the user has seen the error message in case one is shown
    slotHideErrorMessageWidget();
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

void SetupWizardWindow::setPaginationEntries(const QStringList &paginationEntries)
{
    _ui->pagination->setEntries(paginationEntries);
}

void SetupWizardWindow::slotUpdateNextButton()
{
    _ui->nextButton->setEnabled(_currentPage->validateInput());
}

bool SetupWizardWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!_transitioning) {
        // whenever the user types another character somewhere inside the page, we can re-evaluate whether to enable the next button
        switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            slotUpdateNextButton();
            break;
        default:
            break;
        }

        if (obj == _currentPage.data() || obj == this) {
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
    slotStartTransition();
    emit nextButtonClicked(_ui->pagination->activePageIndex());
}
}
