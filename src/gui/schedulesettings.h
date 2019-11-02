#ifndef SCHEDULESETTINGS_H
#define SCHEDULESETTINGS_H

#include <QLoggingCategory>
#include <QDialog>
#include <QTableWidget>
#include <QAbstractButton>
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
  class ScheduleSettings : public QDialog
  {
    Q_OBJECT

    public:
    explicit ScheduleSettings(QTimer *scheduleTimer, QWidget *parent = nullptr);
    ~ScheduleSettings();
    
    static const unsigned short int SCHEDULE_TIME = 5000;

  private slots:
    void okButton();
    void cancelButton();
    void resetButton(QAbstractButton *button);
    void saveScheduleSettings();
    void loadScheduleSettings();
    void resetScheduleSettings();

  private:
    Ui::ScheduleSettings *_ui;
    bool _currentlyLoading;
    QTimer *_scheduleTimer;
  };


} // namespace OCC
#endif // SCHEDULESETTINGS_H
