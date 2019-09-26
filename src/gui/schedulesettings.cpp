#include "schedulesettings.h"
#include "ui_schedulesettings.h"

#include "configfile.h"
#include "application.h"
#include "accountmanager.h"

#include <QScopedValueRollback>
#include <QPushButton>
#include <QDate>
#include <QTime>
#include <QTableWidgetItem>

#define QTLEGACY (QT_VERSION < QT_VERSION_CHECK(5,9,0))

#if !(QTLEGACY)
#include <QOperatingSystemVersion>
#endif

namespace OCC {

  Q_LOGGING_CATEGORY(lcScheduler, "nextcloud.gui.scheduler", QtInfoMsg)

  const char propertyAccountC[] = "oc_account";

  
  ScheduleSettings::ScheduleSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ScheduleSettings)
    , _currentlyLoading(false)
  {
    _ui->setupUi(this);

    // create label items
    QTableWidget *runTableWidget = _ui->runTableWidget;
    QTableWidgetItem *newItemRun = new QTableWidgetItem();    
    newItemRun->setFlags(newItemRun->flags() &  ~Qt::ItemIsEnabled);
    runTableWidget->setItem(0, 0, newItemRun);
    runTableWidget->selectRow(0);
    QTableWidget *suspendTableWidget = _ui->suspendTableWidget;
    QTableWidgetItem *newItemSuspend = new QTableWidgetItem();    
    newItemSuspend->setBackground(Qt::white);
    newItemSuspend->setFlags(newItemSuspend->flags() &  ~Qt::ItemIsEnabled);
    suspendTableWidget->setItem(0, 0, newItemSuspend);
    
    // create items table disabled by default and strech them
    QTableWidget *tableWidget = _ui->scheduleTableWidget;
    QHeaderView* headerH = tableWidget->horizontalHeader();
    QHeaderView* headerV = tableWidget->verticalHeader();
    headerH->setSectionResizeMode(QHeaderView::Stretch);    
    headerV->setSectionResizeMode(QHeaderView::Stretch);
    
    
    // create internal table for checking synchronization
    int rows = tableWidget->rowCount();
    int cols = tableWidget->columnCount();
    QStringList horzHeaders, verHeaders;
    for (int idx=0; idx<cols; idx++)
      horzHeaders << QString::number(idx);
    for (int idx=0; idx<rows; idx++)
      verHeaders << tableWidget->verticalHeaderItem(idx)->text().toLatin1();
    _timerTable = new QTableWidget(rows,cols);
    _timerTable->setHorizontalHeaderLabels(horzHeaders);
    _timerTable->setVerticalHeaderLabels(verHeaders);

    // fill tables with items
    for (int idx = 0; idx<rows; idx++){
      for (int idj = 0; idj<cols; idj++){
        QTableWidgetItem *newItem = new QTableWidgetItem();
        QTableWidgetItem *newItemTimer = new QTableWidgetItem();
        newItem->setBackground(Qt::white);
        tableWidget->setItem(idx, idj, newItem);
        _timerTable->setItem(idx, idj, newItemTimer);
      }
    }   

    // create timer to check configuration every 5 seconds
    _scheduleTimer = new QTimer(this);
    connect(_scheduleTimer, &QTimer::timeout, this, &ScheduleSettings::checkSchedule);
       
    // connect with events
    connect(_ui->saveButton, &QPushButton::clicked, this, &ScheduleSettings::saveScheduleSettings);
    connect(_ui->resetButton, &QPushButton::clicked, this, &ScheduleSettings::resetScheduleSettings);
    connect(_ui->enableScheduleCheckBox, &QCheckBox::clicked, this, &ScheduleSettings::changedScheduleSettings);
    connect(_ui->scheduleTableWidget, &QTableWidget::cellPressed, this, &ScheduleSettings::changedScheduleSettings);

    // load settings stored in config file
    loadScheduleSettings();    
  }

  
  ScheduleSettings::~ScheduleSettings()
  {
    delete _scheduleTimer;
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
      _scheduleTimer->start(SCHEDULE_TIME);
    }else{
      qCInfo(lcScheduler) << "Sync Scheduler disabled";
      _scheduleTimer->stop();
    }
    cfgFile.getScheduleTable(*table);
    
    // finally disable reset and save buttons
    _ui->resetButton->setEnabled(false);
    _ui->saveButton->setEnabled(false);
  }


  void ScheduleSettings::resetScheduleSettings()
  {
    loadScheduleSettings();
  }

  void ScheduleSettings::changedScheduleSettings()
  {
    _ui->resetButton->setEnabled(true);
    _ui->saveButton->setEnabled(true);
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

    //activate/deactivate sync depending the day of the week and the hour
    QDate date = QDate::currentDate();
    int day = date.dayOfWeek();
    QTime time = QTime::currentTime();
    int hour = time.hour();
    QTableWidgetItem *item = _timerTable->item(day-1, hour);
    if( item->isSelected() ){
      qCDebug(lcScheduler) << "Start sync: " << day << " - " << hour;
      this->setPauseOnAllFoldersHelper(false);
    }else{
      qCDebug(lcScheduler) << "Stop Sync: " << day << " - " << hour;
      this->setPauseOnAllFoldersHelper(true);
    }
  }

  void ScheduleSettings::setPauseOnAllFoldersHelper(bool pause)
  {
    // this funcion is a copy of ownCloudGui::setPauseOnAllFoldersHelper(bool pause)
    QList<AccountState *> accounts;
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
      accounts.append(account.data());
    } else {
      foreach (auto a, AccountManager::instance()->accounts()) {
        accounts.append(a.data());
      }
    }
    foreach (Folder *f, FolderMan::instance()->map()) {
      if (accounts.contains(f->accountState())) {
        f->setSyncPaused(pause);
        if (pause) {
          f->slotTerminateSync();
        }
      }
    }
  }
  
} // namespace OCC
