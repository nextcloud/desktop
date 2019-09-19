#ifndef MIRALL_SCHEDULESETTINGS_H
#define MIRALL_SCHEDULESETTINGS_H

#include <QWidget>
#include <QPointer>

namespace OCC {

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

private:
    Ui::ScheduleSettings *_ui;
    bool _currentlyLoading;
};


} // namespace OCC
#endif // MIRALL_SCHEDULESETTINGS_H
