#include "dataprotectionsettingspage.h"
#include "configfile.h"
#include "buttonstyle.h"
#include "guiutility.h"
#include "theme.h"
#include "ui_dataprotectionsettingspage.h"
#include "wizard/owncloudwizard.h"

namespace OCC{

    DataProtectionSettingsPage::DataProtectionSettingsPage(OwncloudWizard *ocWizard)
        : QWizardPage()
        , _ui(new Ui::DataProtectionSettingsPage)
        , _ocWizard(ocWizard)
    {
        setupUi();
    }

    DataProtectionSettingsPage::~DataProtectionSettingsPage() = default;

    void DataProtectionSettingsPage::setupUi()
    {
        _ui->setupUi(this);
        setupPage();
    }

    void DataProtectionSettingsPage::initializePage()
    {
        customizeStyle();
    }

    void DataProtectionSettingsPage::setupPage()
    {
        ConfigFile cfgFile;

        connect(_ui->backButton, &QPushButton::clicked, this, [this, &cfgFile]() {
            _ui->anonymousDataCheckBox->setChecked(false);
            _ocWizard->back();
        });

        connect(_ui->saveButton, &QPushButton::clicked, this, [this, &cfgFile](){
            cfgFile.setSendData(_ui->anonymousDataCheckBox->isChecked());
            _ocWizard->next();
        });

        connect(_ui->necessaryDataCheckBox, &QCheckBox::clicked, this, [this](){
            _ui->necessaryDataCheckBox->setChecked(true);
        });

        _ui->descriptionLabel->setText(tr("We collect anonymized data to optimize our app. We use software solutions from various partners for this purpose. We want to give you full transparency and freedom of choice regarding the collection and processing of your anonymized usage. You can change your settings at any time under the menu item Data Protection."));
    
        _ui->anonymousDataCheckBox->setChecked(false);
    }

    int DataProtectionSettingsPage::nextId() const
    {
        return _nextPage;
    }

    void DataProtectionSettingsPage::customizeStyle()
    {
        _ocWizard->setFixedSize(626, 480);

        _ui->mainVBox->setContentsMargins(32, 0, 32, 0);

        _ui->necessaryDataCheckBox->setChecked(true);

        _ui->necessaryDataCheckBox->setStyleSheet(
            QStringLiteral("QCheckBox { %1; }").arg(
                WLTheme.fontConfigurationCss(
                    WLTheme.settingsFont(),
                    WLTheme.settingsTextSize(),
                    WLTheme.settingsTitleWeight600(),
                    WLTheme.folderWizardSubtitleColor()
                )
            ));

            _ui->anonymousDataCheckBox->setStyleSheet(
                QStringLiteral("QCheckBox { %1; }").arg(
                    WLTheme.fontConfigurationCss(
                        WLTheme.settingsFont(),
                        WLTheme.settingsTextSize(),
                        WLTheme.settingsTitleWeight600(),
                        WLTheme.folderWizardSubtitleColor()
                    )
                ));

        _ui->descriptionLabel->setStyleSheet(
            QStringLiteral("QLabel { %1; margin-top: %2; margin-bottom: %2; }").arg(
                WLTheme.fontConfigurationCss(
                    WLTheme.settingsFont(),
                    WLTheme.settingsTextSize(),
                    WLTheme.settingsTextWeight(),
                    WLTheme.black()
                ),
                "24"
            )
        );

        _ui->necessaryDataLabel->setStyleSheet(
            QStringLiteral("QLabel { %1; margin-left: %2; margin-bottom: %3; }").arg(
                WLTheme.fontConfigurationCss(
                    WLTheme.settingsFont(),
                    WLTheme.settingsTextSize(),
                    WLTheme.settingsTextWeight(),
                    WLTheme.black()
                ),
                "16", 
                WLTheme.smallMargin()
            )
        );

        _ui->anonymousDataLabel->setStyleSheet(
            QStringLiteral("QLabel { %1; margin-left: %2; margin-bottom: %3; }").arg(
                WLTheme.fontConfigurationCss(
                    WLTheme.settingsFont(),
                    WLTheme.settingsTextSize(),
                    WLTheme.settingsTextWeight(),
                    WLTheme.black()
                ),
                "16", 
                "48"
            )
        );

        _ui->buttonLayout->setAlignment(Qt::AlignCenter);
        _ui->buttonLayout->setSpacing(16);
        _ui->buttonLayout->setContentsMargins(0, 16, 0, 16);

        _ui->backButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        _ui->saveButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        _ui->saveButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));
    }
}