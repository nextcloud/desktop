#ifndef BUTTONSTYLESTRATEGY_H
#define BUTTONSTYLESTRATEGY_H

#include "buttonstyle.h"
#include <QString>
#include <QWidget>


class ButtonStyleStrategy
{
public:
    virtual ~ButtonStyleStrategy() = default;

    static OCC::ButtonStyle& getButtonStyle(const QWidget *widget, const QStyleOptionButton *option)
    {
        OCC::ButtonStyleName buttonStyleName;
        if(widget != nullptr)
        {
            buttonStyleName = determineButtonStyleName(widget, option);
        }
        else
        {
            buttonStyleName = OCC::ButtonStyleName::Secondary;
        }
        
        switch (buttonStyleName)
        {
            case OCC::ButtonStyleName::MoreOptions:
                return OCC::MoreOptionsButtonStyle::GetInstance();
            case OCC::ButtonStyleName::Primary:
                return OCC::PrimaryButtonStyle::GetInstance();
            case OCC::ButtonStyleName::Secondary:
            default:
                return OCC::SecondaryButtonStyle::GetInstance();
        }
    }

    static OCC::ButtonStyleName determineButtonStyleName(const QWidget *widget, const QStyleOptionButton *option)
    {
        QVariant propertyValue = widget->property("buttonStyle");        
        if(propertyValue.isValid()){             

            return propertyValue.value<OCC::ButtonStyleName>();
        }

        return getButtonStyleNameByObjectName(widget);
    }

    static OCC::ButtonStyleName getButtonStyleNameByObjectName(const QWidget *widget)
    {
        static const QMap<QString, OCC::ButtonStyleName> buttonStyleMap = {
            {"qt_wizard_finish", OCC::ButtonStyleName::Primary}
        };

        QString buttonName = widget->objectName();
        return buttonStyleMap.value(buttonName, OCC::ButtonStyleName::Secondary);
    }
};

#endif // BUTTONSTYLESTRATEGY_H