#include "schedulesettings.h"
#include "ui_schedulesettings.h"

#include "configfile.h"
#include "application.h"
#include "accountmanager.h"

#include <QScopedValueRollback>
#include <QPushButton>
#include <QTableWidgetItem>

#define QTLEGACY (QT_VERSION < QT_VERSION_CHECK(5,9,0))

#if !(QTLEGACY)
#include <QOperatingSystemVersion>
#endif

namespace OCC {

  Q_LOGGING_CATEGORY(lcScheduler, "nextcloud.gui.scheduler", QtInfoMsg)

  const char propertyAccountC[] = "oc_account";

  
  ScheduleSettings::ScheduleSettings(QTimer *scheduleTimer, QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::ScheduleSettings)
    , _currentlyLoading(false)
    , _scheduleTimer(scheduleTimer)
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
    
    
    // fill tables with items
    int rows = tableWidget->rowCount();
    int cols = tableWidget->columnCount();    
    for (int idx = 0; idx<rows; idx++){
      for (int idj = 0; idj<cols; idj++){
        QTableWidgetItem *newItem = new QTableWidgetItem();
        QTableWidgetItem *newItemTimer = new QTableWidgetItem();
        newItem->setBackground(Qt::white);
        newItem->setFlags(newItem->flags() &  ~Qt::ItemIsEditable);
        tableWidget->setItem(idx, idj, newItem);
      }
    }   
       
    // connect with events
    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &ScheduleSettings::okButton);
    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &ScheduleSettings::cancelButton);
    connect(_ui->buttonBox, &QDialogButtonBox::clicked, this, &ScheduleSettings::resetButton);

    // load settings stored in config file
    loadScheduleSettings();    
  }

  
  ScheduleSettings::~ScheduleSettings()
  {
    delete _ui;
  }

  void ScheduleSettings::okButton(){
    this->saveScheduleSettings();
    accept();
  }
  void ScheduleSettings::cancelButton(){
    this->resetScheduleSettings();
    reject();
  }
  void ScheduleSettings::resetButton(QAbstractButton *button){
    if(_ui->buttonBox->buttonRole(button) != QDialogButtonBox::ResetRole)
      return;
    this->resetScheduleSettings();
  }
  
  void ScheduleSettings::loadScheduleSettings()
  {
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;
    QTableWidget* table = _ui->scheduleTableWidget;
    _ui->enableScheduleCheckBox->setChecked(cfgFile.getScheduleStatus());
    if(cfgFile.getScheduleStatus()){
      qCInfo(lcScheduler) << "Sync Scheduler enabled";
      _scheduleTimer->start(ScheduleSettings::SCHEDULE_TIME);
    }else{
      qCInfo(lcScheduler) << "Sync Scheduler disabled";
      _scheduleTimer->stop();
    }
    cfgFile.getScheduleTable(*table);
    
  }


  void ScheduleSettings::resetScheduleSettings()
  {
    loadScheduleSettings();
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
