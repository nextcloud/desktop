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
#include <QDate>
#include <QTime>
#include <QDebug>
#include <QTableWidgetItem>

#define QTLEGACY (QT_VERSION < QT_VERSION_CHECK(5,9,0))

#if !(QTLEGACY)
#include <QOperatingSystemVersion>
#endif

namespace OCC {

  Q_LOGGING_CATEGORY(lcScheduler, "nextcloud.gui.scheduler", QtInfoMsg)
  
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
    QStringList horzHeaders, verHeaders;
    for (int idx=0; idx<24; idx++)
      horzHeaders << QString::number(idx);
    verHeaders << "Mon" << "Tue" << "Wed" << "Thu" << "Fri" << "Sat" << "Sun";
    _timerTable = new QTableWidget(rows,cols);
    _timerTable->setHorizontalHeaderLabels(horzHeaders);
    _timerTable->setVerticalHeaderLabels(verHeaders);
    for (int idx = 0; idx<rows; idx++){
      for (int idj = 0; idj<cols; idj++){
        QTableWidgetItem *newItem = new QTableWidgetItem();
        QTableWidgetItem *newItemTimer = new QTableWidgetItem();
        newItem->setBackground(Qt::white);
        tableWidget->setItem(idx, idj, newItem);
        _timerTable->setItem(idx, idj, newItemTimer);
      }
    }

    

    //create timer to check configuration every 5 seconds
    _scheduleTimer = new QTimer(this);
    connect(_scheduleTimer, SIGNAL(timeout()), this, SLOT(checkSchedule()));
    _scheduleTimer->start(5000);
    
    // load settings stored in config file
    loadScheduleSettings();
    
    // connect with events
    connect(_ui->saveButton, &QPushButton::clicked, this, &ScheduleSettings::saveScheduleSettings);
  }

  ScheduleSettings::~ScheduleSettings()
  {
    delete _timerTable;
    delete _ui;
  }

  void ScheduleSettings::loadScheduleSettings()
  {
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;
    QTableWidget* table = _ui->scheduleTableWidget;
    _ui->enableScheduleCheckBox->setChecked(cfgFile.getScheduleStatus());
    if(cfgFile.getScheduleStatus()){
      qCInfo(lcScheduler) << "Sync Scheduler enabled";
    }else{
      qCInfo(lcScheduler) << "Sync Scheduler disabled";
    }
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

  void ScheduleSettings::checkSchedule(){
    ConfigFile cfgFile;
    cfgFile.getScheduleTable(*_timerTable);
    bool enabled = cfgFile.getScheduleStatus();

    if( enabled ){
      QDate date = QDate::currentDate();
      int day = date.dayOfWeek();
      QTime time = QTime::currentTime();
      int hour = time.hour();
      QTableWidgetItem *item = _timerTable->item(day-1, hour);
      if( item->isSelected() ){
        qCInfo(lcScheduler) << "Start sync: " << day << " - " << hour;
      }else{
        qCInfo(lcScheduler) << "Not start sync: " << day << " - " << hour;
      }
    }else{
      qCInfo(lcScheduler) << "Stop sync";
    }
  }

} // namespace OCC
