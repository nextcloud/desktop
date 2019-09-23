#ifndef MIRALL_SCHEDULESETTINGS_H
#define MIRALL_SCHEDULESETTINGS_H

#include <QLoggingCategory>
#include <QWidget>
#include <QTableWidget>
#include <QTimer>

namespace OCC {

  Q_DECLARE_LOGGING_CATEGORY(lcScheduler)
  
  namespace Ui {
    class ScheduleSettings;
  }

  /**
   * @brief The ScheduleSettings class
   * @ingroup gui
   */
  class ScheduleSettings : public QWidget
  {
    Q_OBJECT

    public:
    explicit ScheduleSettings(QWidget *parent = nullptr);
    ~ScheduleSettings();

  private slots:
    void saveScheduleSettings();
    void loadScheduleSettings();
    void checkSchedule();

  private:
    void setPauseOnAllFoldersHelper(bool pause);
    
    Ui::ScheduleSettings *_ui;
    bool _currentlyLoading;
    QTimer *_scheduleTimer;
    QTableWidget *_timerTable;
    
    const unsigned short int SCHEDULE_TIME = 5000;
  };


} // namespace OCC
#endif // MIRALL_SCHEDULESETTINGS_H
