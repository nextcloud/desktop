#ifndef MOREOPTIONSBUTTONSTYLEHELPER_H
#define MOREOPTIONSBUTTONSTYLEHELPER_H

#include <QSize>

class QPainter;
class QStyleOptionButton;
class QWidget;
class QPixmap;
class QColor;
class QIcon;

class MoreOptionsButtonStyleHelper
{
public:
    void setupPainterForToolButtonShape(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget);
    void drawToolButtonShape(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget);
    void adjustIconColor(QStyleOptionButton *option, const QWidget *widget);

private:
    QPixmap tintPixmap(const QPixmap &src, const QColor &color) const;
    QSize getLargestIconSize(const QIcon &icon) const;

};
#endif // MOREOPTIONSBUTTONSTYLEHELPER_H