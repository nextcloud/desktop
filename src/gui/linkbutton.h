// linkbutton.h
#ifndef LINKBUTTON_H
#define LINKBUTTON_H

#include <QLabel>
#include <QWidget>
#include <QMouseEvent>
#include <QEvent>
namespace OCC {
    class LinkButton : public QLabel
    {
        Q_OBJECT

    public:
        explicit LinkButton(QWidget* parent = nullptr);

    signals:
        void clicked();

    protected:
        void mousePressEvent(QMouseEvent* event);
        void changeEvent(QEvent* event) override;

    private:
        void customizeStyle();
    };
}
#endif // LINKBUTTON_H
