#include "moreoptionsbuttonstylehelper.h"

#include "buttonstylestrategy.h"
#include "buttonstyle.h"
#include "ionostheme.h"
#include <QPainter>
#include <QStyleOptionButton>
#include <QWidget>


void MoreOptionsButtonStyleHelper::setupPainterForToolButtonShape(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget)
{
    OCC::ButtonStyle& style = ButtonStyleStrategy::getButtonStyle(widget, option);

    // Disabled
    if (!(option->state & QStyle::State_Enabled)) {
        painter->setPen(QColor(style.buttonDisabledBorderColor()));
        painter->setBrush(QColor(style.buttonDisabledColor()));
    }
    //Pressed
    else if (option->state & QStyle::State_Sunken)
    {
        painter->setPen(QColor(style.buttonPressedBorderColor()));
        painter->setBrush(QColor(style.buttonPressedColor()));
    }
    // Hover
    else if(option->state & QStyle::State_MouseOver)
    {
        painter->setPen(QColor(style.buttonHoverBorderColor()));
        painter->setBrush(QColor(style.buttonHoverColor()));
    }
    // Focused
    else if (option->state & QStyle::State_HasFocus) {
        painter->setPen(QColor(style.buttonFocusedBorderColor()));
        painter->setBrush(QColor(style.buttonFocusedColor()));
    }
    // Else - Just beeing there
    else {
        painter->setPen(QColor(style.buttonDefaultBorderColor()));
        painter->setBrush(QColor(style.buttonDefaultColor()));
    }
}

void MoreOptionsButtonStyleHelper::drawToolButtonShape(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget)
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    setupPainterForToolButtonShape(option, painter, widget);
    const int radius =  option->rect.height() / 2;
    painter->drawRoundedRect(option->rect, radius,radius);
    painter->restore();
}

QPixmap MoreOptionsButtonStyleHelper::tintPixmap(const QPixmap &src, const QColor &color) const{
    QPixmap result(src.size());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(0, 0, src);

    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(result.rect(), color);

    painter.end();
    return result;
}

QSize MoreOptionsButtonStyleHelper::getLargestIconSize(const QIcon &icon) const{
    QList<QSize> availableSizes = icon.availableSizes();

    if (availableSizes.isEmpty()) {
        return QSize();
    }

    QSize largestSize;
    int maxDimension = 0;

    for (const QSize &size : availableSizes) {
        if (size.width() > maxDimension) {
            maxDimension = size.width();
            largestSize = size;
        }
    }

    return largestSize;
}

void MoreOptionsButtonStyleHelper::adjustIconColor(QStyleOptionButton *option, const QWidget *widget)
{
    QColor iconColor;
    OCC::ButtonStyle& style = ButtonStyleStrategy::getButtonStyle(widget, option);

    if (!(option->state & QStyle::State_Enabled)) {
        iconColor = style.buttonDisabledFontColor();
    }
    else if(option->state & QStyle::State_MouseOver)
    {
        iconColor = QColor(style.buttonDefaultColor());
    }
    else
    {
        iconColor = QColor(style.buttonHoverColor());
    }

    QIcon icon = option->icon;
    QSize iconSize = getLargestIconSize(icon);
    QPixmap pixmap = icon.pixmap(iconSize);

    QPixmap coloredPixmap = tintPixmap(pixmap, iconColor);

    option->icon = (QIcon(coloredPixmap));
}