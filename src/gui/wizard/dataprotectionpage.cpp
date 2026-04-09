#include "dataprotectionpage.h"
#include "buttonstyle.h"
#include "linkbutton.h"
#include "guiutility.h"
#include "theme.h"
#include "ui_dataprotectionpage.h"
#include "wizard/owncloudwizard.h"
#include <QDesktopServices>

namespace OCC{

    DataProtectionPage::DataProtectionPage(OwncloudWizard *ocWizard)
        : QWizardPage()
        , _ui(new Ui::DataProtectionPage)
        , _ocWizard(ocWizard)
    {
        setupUi();
    }

    DataProtectionPage::~DataProtectionPage() = default;

    void DataProtectionPage::setupUi()
    {
        _ui->setupUi(this);
    }

    void DataProtectionPage::initializePage()
    {
        ConfigFile cfgFile;
        cfgFile.setSendData(true);
        connect(_ui->agreeButton, &QPushButton::clicked, this, [this]() {
            _nextPage = WizardCommon::Page_AdvancedSetup; 
            _ocWizard->next();
        });

        connect(_ui->settingsButton, &QPushButton::clicked, this, [this](){
            _nextPage = WizardCommon::Page_DataProtectionSettings; 
            _ocWizard->next();
        });

        QString dataProtectionLogo = QString();

#if defined(IONOS_WL_BUILD)
    dataProtectionLogo = Theme::hidpiFileName(":/client/theme/colored/ionos-data-protection-logo.png");
#else defined(STRATO_WL_BUILD)
    dataProtectionLogo = Theme::hidpiFileName(":/client/theme/colored/strato-data-protection-logo.png");
#endif

        _ui->logoLabel->setPixmap(Theme::hidpiFileName(dataProtectionLogo));
        _ui->descriptionLabel->setText(tr("This application uses tracking technologies. By clicking on Agree, you accept the processing of your anonymized data. You can adjust your choices at any time via the settings. <br/> <br/>Information on data processing and more can be found in our <a href='https://wl.hidrive.com/easy/0005'>privacy policy</a>."));
        _ui->descriptionLabel->setOpenExternalLinks(true);
        _ui->descriptionLabel->setTextFormat(Qt::RichText);

        customizeStyle();
    }

    int DataProtectionPage::nextId() const
    {
        return _nextPage;
    }

    void DataProtectionPage::customizeStyle()
    {
        _ocWizard->setFixedSize(626, 470);
        _ui->mainVBox->setContentsMargins(24, 0, 24, 24);   

        _ui->logoLabel->setAlignment(Qt::AlignHCenter);
        _ui->logoLabel->setMargin(8);

        _ui->descriptionLabel->setStyleSheet(
            QStringLiteral("QLabel { %1; margin-left: %2; margin-right: %2; margin-bottom: %2; }").arg(
                WLTheme.fontConfigurationCss(
                    WLTheme.settingsFont(),
                    WLTheme.settingsTextSize(),
                    WLTheme.settingsTextWeight(),
                    WLTheme.black()
                ),
                "32"
            )
        );

        _ui->agreeButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        _ui->agreeButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));

        _ui->settingsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

        _ui->buttonLayout->setAlignment(Qt::AlignCenter);
        _ui->buttonLayout->setSpacing(16);
    }
}