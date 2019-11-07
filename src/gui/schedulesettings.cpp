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
    , _firstPress(true)
  {
    _ui->setupUi(this);

    // create label items
    QTableWidget *syncTableWidget = _ui->syncTableWidget;
    QTableWidgetItem *newItemSync = new QTableWidgetItem();    
    newItemSync->setFlags(newItemSync->flags() &  ~Qt::ItemIsEnabled);
    syncTableWidget->setItem(0, 0, newItemSync);
    syncTableWidget->selectRow(0);
    QTableWidget *pauseTableWidget = _ui->pauseTableWidget;
    QTableWidgetItem *newItemPause = new QTableWidgetItem();    
    newItemPause->setBackground(Qt::white);
    newItemPause->setFlags(newItemPause->flags() &  ~Qt::ItemIsEnabled);
    pauseTableWidget->setItem(0, 0, newItemPause);
    
    // fill table with items
    QTableWidget *tableWidget = _ui->scheduleTableWidget;
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
    // set style of table headers
    QString styleSheetHeader = "::section {" // "QHeaderView::section {"
      "background-color: rgb(255,255,255);"
      "text-align: right;}";
    QHeaderView* headerH = tableWidget->horizontalHeader();
    QHeaderView* headerV = tableWidget->verticalHeader();
    headerH->setSectionResizeMode(QHeaderView::Stretch);
    headerV->setSectionResizeMode(QHeaderView::Stretch);    
    headerH->setStyleSheet(styleSheetHeader);
    headerV->setStyleSheet(styleSheetHeader);
    QAbstractButton *button =  tableWidget->findChild<QAbstractButton *>();
    button->setStyleSheet("background-color: rgb(255,255,255);");
    
    // connect with events
    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &ScheduleSettings::okButton);
    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &ScheduleSettings::cancelButton);
    connect(_ui->scheduleTableWidget, &QTableWidget::cellPressed, this, &ScheduleSettings::resetTable);

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
  void ScheduleSettings::resetTable(){
    if( _firstPress ){
      QTableWidget *tableWidget = _ui->scheduleTableWidget;    
      int rows = tableWidget->rowCount();
      int cols = tableWidget->columnCount();    
      for (int idx = 0; idx<rows; idx++){
        for (int idj = 0; idj<cols; idj++){
          tableWidget->item(idx, idj)->setSelected(false);
          tableWidget->item(idx, idj)->setBackground(Qt::white);
        }
      }
      _firstPress = false;
    }
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
