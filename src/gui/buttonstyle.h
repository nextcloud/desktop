
#ifndef _BUTTONSTYLE_H
#define _BUTTONSTYLE_H
 
#include "whitelabeltheme.h"
#include <QMetaType>
#include <QString>

namespace OCC{

enum class ButtonStyleName {
    Primary,
    Secondary,
    MoreOptions,
};
OCSYNC_EXPORT Q_NAMESPACE;
Q_ENUM_NS(ButtonStyleName); 
}
Q_DECLARE_METATYPE(OCC::ButtonStyleName);

namespace OCC{
class ButtonStyle 
{
protected:
    ButtonStyle()
    {
        qRegisterMetaType<OCC::ButtonStyleName>("OCC::ButtonStyleName");
    }
    ~ButtonStyle() {}

public:

    // Default
    virtual QString buttonDefaultColor() const = 0;
    virtual QString buttonDefaultBorderColor() const = 0;
    // Hover
    virtual QString buttonHoverColor() const = 0;
    virtual QString buttonHoverBorderColor() const = 0;
    // Pressed
    virtual QString buttonPressedColor() const = 0;
    virtual QString buttonPressedBorderColor() const = 0;
    // Disabled
    virtual QString buttonDisabledColor() const = 0;
    virtual QString buttonDisabledBorderColor() const = 0;
    // Focused
    virtual QString buttonFocusedColor() const = 0;
    virtual QString buttonFocusedBorderColor() const = 0;
    // Font
    virtual QString buttonDisabledFontColor() const = 0;
    virtual QString buttonFontColor() const = 0;
    //Icon
    virtual QString buttonIconDefaultColor() const = 0;
    virtual QString buttonIconHoverColor() const = 0;
};

class PrimaryButtonStyle : public ButtonStyle {
private: 
    PrimaryButtonStyle()
    {
    }
    ~PrimaryButtonStyle() {}
public:

    PrimaryButtonStyle(PrimaryButtonStyle &other) = delete;
    void operator=(const PrimaryButtonStyle &) = delete;

    static PrimaryButtonStyle& GetInstance() {
        static PrimaryButtonStyle instance;
        return instance;
    }

    // Default
    QString buttonDefaultColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryColor();
    }

    QString buttonDefaultBorderColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryColor();
    }

    //Hover
    QString buttonHoverColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryHoverColor();
    }

    QString buttonHoverBorderColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryHoverColor();
    }

    // Pressed
    QString buttonPressedColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryPressedColor();
    }

    QString buttonPressedBorderColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryPressedColor();
    }

    // Disabled
    QString buttonDisabledColor() const override 
    {
        return OCC::WLTheme.buttonDisabledColor();
    }

    QString buttonDisabledBorderColor() const override 
    {
        return OCC::WLTheme.buttonDisabledColor();
    }

    // Focused
    QString buttonFocusedColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryColor();
    }

    QString buttonFocusedBorderColor() const override 
    {
        return OCC::WLTheme.buttonPrimaryFocusedBorderColor();
    }

    // Font
    QString buttonDisabledFontColor() const override 
    {
        return OCC::WLTheme.buttonDisabledFontColor();
    }

    QString buttonFontColor() const override 
    {
        return OCC::WLTheme.white();
    }

    // Icon (Three Dots)
    QString buttonIconDefaultColor() const override
    {
        return OCC::WLTheme.white();
    }

    QString buttonIconHoverColor() const override
    {
        return OCC::WLTheme.white();
    }
};

class SecondaryButtonStyle : public ButtonStyle {
protected: 
    SecondaryButtonStyle()
    {
    }
    ~SecondaryButtonStyle() {}
public:

    SecondaryButtonStyle(SecondaryButtonStyle &other) = delete;
    void operator=(const SecondaryButtonStyle &) = delete;

    static SecondaryButtonStyle& GetInstance() {
        static SecondaryButtonStyle instance; 
        return instance;
    }

    // Default
    QString buttonDefaultColor() const override 
    {
        return OCC::WLTheme.buttonSecondaryColor();
    }

    QString buttonDefaultBorderColor() const override 
    {
        return OCC::WLTheme.buttonSecondaryBorderColor();
    }

    // Hover
    QString buttonHoverColor() const override 
    {
        return OCC::WLTheme.buttonSecondaryHoverColor();
    }

    QString buttonHoverBorderColor() const override 
    {
        return OCC::WLTheme.buttonSecondaryBorderColor();
    }

    // Pressed
    QString buttonPressedColor() const override 
    {
        return OCC::WLTheme.buttonSecondaryPressedColor();
    }

    QString buttonPressedBorderColor() const override 
    {
        return OCC::WLTheme.buttonSecondaryBorderColor();
    }

    // Disabled
    QString buttonDisabledColor() const override 
    {
        return OCC::WLTheme.buttonDisabledColor();
    }

    QString buttonDisabledBorderColor() const override 
    {
        return OCC::WLTheme.buttonDisabledColor();
    }

    // Focused 
    QString buttonFocusedColor() const override 
    {
        return OCC::WLTheme.white();
    }

    QString buttonFocusedBorderColor() const override 
    { 
        return OCC::WLTheme.buttonSecondaryFocusedBorderColor();
    }

    // Font
    QString buttonDisabledFontColor() const override 
    {
        return OCC::WLTheme.buttonDisabledFontColor();
    }

    QString buttonFontColor() const override 
    {
        return OCC::WLTheme.black();
    }

    // Icon (Three Dots)
    QString buttonIconDefaultColor() const override
    {
        return OCC::WLTheme.white();
    }

    QString buttonIconHoverColor() const override
    {
        return OCC::WLTheme.white();
    }
};

class MoreOptionsButtonStyle : public ButtonStyle {
protected: 
    MoreOptionsButtonStyle()
    {
    }
    ~MoreOptionsButtonStyle() {}
public:

    MoreOptionsButtonStyle(MoreOptionsButtonStyle &other) = delete;
    void operator=(const MoreOptionsButtonStyle &) = delete;

    static MoreOptionsButtonStyle& GetInstance() {
        static MoreOptionsButtonStyle instance; 
        return instance;
    }

    // Default
    QString buttonDefaultColor() const override 
    {
        return OCC::WLTheme.white();
    }

    QString buttonDefaultBorderColor() const override 
    {
        return OCC::WLTheme.white();
    }

    // Hover
    QString buttonHoverColor() const override 
    {
        return OCC::WLTheme.buttonHoveredColor();
    }

    QString buttonHoverBorderColor() const override 
    {
        return OCC::WLTheme.buttonHoveredColor();
    }

    // Pressed
    QString buttonPressedColor() const override 
    {
        return OCC::WLTheme.buttonPressedColor();
    }

    QString buttonPressedBorderColor() const override 
    {
        return OCC::WLTheme.buttonPressedColor();
    }

    // Disabled
    QString buttonDisabledColor() const override 
    {
        return OCC::WLTheme.buttonDisabledColor();
    }

    QString buttonDisabledBorderColor() const override 
    {
        return OCC::WLTheme.buttonDisabledColor();
    }

    // Focused 
    QString buttonFocusedColor() const override 
    {
        return OCC::WLTheme.white();
    }

    QString buttonFocusedBorderColor() const override 
    { 
        return OCC::WLTheme.black();
    }

    // Font
    QString buttonDisabledFontColor() const override 
    {
        return OCC::WLTheme.buttonDisabledFontColor();
    }

    QString buttonFontColor() const override 
    {
        return OCC::WLTheme.black();
    }

    // Icon (Three Dots)
    QString buttonIconDefaultColor() const
    {
        return OCC::WLTheme.buttonIconColor();
    }

    QString buttonIconHoverColor() const
    {
        return OCC::WLTheme.buttonIconHoverColor();
    }
};
}

#endif // _BUTTONSTYLE_H
