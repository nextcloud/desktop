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
#include <QPushButton>

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

    //Create items table disabled by default
    QTableWidget *tableWidget = _ui->scheduleTableWidget;
    int rows = tableWidget->rowCount();
    int cols = tableWidget->columnCount();
    for (int idx = 0; idx<rows; idx++){
      for (int idj = 0; idj<cols; idj++){
        QTableWidgetItem *newItem = new QTableWidgetItem();
        newItem->setBackground(Qt::white);
        tableWidget->setItem(idx, idj, newItem);
      }
    }

    // load settings stored in config file
    loadScheduleSettings();

    // connect with events
    connect(_ui->saveButton, &QPushButton::clicked, this, &ScheduleSettings::saveScheduleSettings);
}

ScheduleSettings::~ScheduleSettings()
{
    delete _ui;
}

void ScheduleSettings::loadScheduleSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;
    QTableWidget* table = _ui->scheduleTableWidget;
    _ui->enableScheduleCheckBox->setChecked(cfgFile.getScheduleStatus());
    cfgFile.getScheduleTable(*table);
}


void ScheduleSettings::saveScheduleSettings()
{
    if (_currentlyLoading)
        return;
    ConfigFile cfgFile;
    bool isChecked = _ui->enableScheduleCheckBox->isChecked();
    cfgFile.setScheduleStatus(isChecked);    
    QTableWidget* table = _ui->scheduleTableWidget;
    cfgFile.setScheduleTable(*table);
    loadScheduleSettings();
}


} // namespace OCC
