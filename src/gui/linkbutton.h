// linkbutton.h
#ifndef LINKBUTTON_H
#define LINKBUTTON_H

#include <QLabel>
#include <QWidget>
#include <QMouseEvent>
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
    };
}
#endif // LINKBUTTON_H
