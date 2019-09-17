#include "schedulesettings.h"
#include "ui_schedulesettings.h"

#include "theme.h"
#include "configfile.h"
#include "application.h"
#include "configfile.h"
#include "owncloudsetupwizard.h"

#include "updater/updater.h"
#include "updater/ocupdater.h"
#include "common/utility.h"

#include "config.h"

#include <QNetworkProxy>
#include <QDir>
#include <QScopedValueRollback>

#define QTLEGACY (QT_VERSION < QT_VERSION_CHECK(5,9,0))

#if !(QTLEGACY)
#include <QOperatingSystemVersion>
#endif

namespace OCC {

ScheduleSettings::ScheduleSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ScheduleSettings)
    , _currentlyLoading(false)
{
    _ui->setupUi(this);

    loadMiscSettings();

    // misc
    connect(_ui->enableScheduleCheckBox, &QAbstractButton::toggled, this, &ScheduleSettings::saveMiscSettings);
}

ScheduleSettings::~ScheduleSettings()
{
    delete _ui;
}

void ScheduleSettings::loadMiscSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;
    _ui->enableScheduleCheckBox->setChecked(cfgFile.schedule());
}


void ScheduleSettings::saveMiscSettings()
{
    if (_currentlyLoading)
        return;
    ConfigFile cfgFile;
    bool isChecked = _ui->enableScheduleCheckBox->isChecked();
    cfgFile.setSchedule(isChecked);
}


} // namespace OCC
