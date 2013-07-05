#ifndef IGNORELISTEDITOR_H
#define IGNORELISTEDITOR_H

#include <QWidget>

class QListWidgetItem;

namespace Mirall {

namespace Ui {
class IgnoreListEditor;
}

class IgnoreListEditor : public QWidget
{
    Q_OBJECT

public:
    explicit IgnoreListEditor(QWidget *parent = 0);
    ~IgnoreListEditor();

private slots:
    void slotItemSelectionChanged();
    void slotRemoveCurrentItem();
    void slotUpdateLocalIgnoreList();
    void slotAddPattern();
    void slotEditPattern(QListWidgetItem*);

private:
    void readIgnoreFile(const QString& file, bool readOnly);
    Ui::IgnoreListEditor *ui;
};

} // namespace Mirall

#endif // IGNORELISTEDITOR_H
