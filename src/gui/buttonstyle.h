
#ifndef _BUTTONSTYLE_H
#define _BUTTONSTYLE_H
 
#include "ionostheme.h"
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
        return OCC::IonosTheme::buttonPrimaryColor();
    }

    QString buttonDefaultBorderColor() const override 
    {
        return OCC::IonosTheme::buttonPrimaryColor();
    }

    //Hover
    QString buttonHoverColor() const override 
    {
        return OCC::IonosTheme::buttonPrimaryHoverColor();
    }

    QString buttonHoverBorderColor() const override 
    {
        return OCC::IonosTheme::buttonPrimaryHoverColor();
    }

    // Pressed
    QString buttonPressedColor() const override 
    {
        return OCC::IonosTheme::buttonPrimaryPressedColor();
    }

    QString buttonPressedBorderColor() const override 
    {
        return OCC::IonosTheme::buttonPrimaryPressedColor();
    }

    // Disabled
    QString buttonDisabledColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledColor();
    }

    QString buttonDisabledBorderColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledColor();
    }

    // Focused
    QString buttonFocusedColor() const override 
    {
        return OCC::IonosTheme::buttonPrimaryColor();
    }

    QString buttonFocusedBorderColor() const override 
    {
        return OCC::IonosTheme::black();
    }

    // Font
    QString buttonDisabledFontColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledFontColor();
    }

    QString buttonFontColor() const override 
    {
        return OCC::IonosTheme::white();
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
        return OCC::IonosTheme::white();
    }

    QString buttonDefaultBorderColor() const override 
    {
        return OCC::IonosTheme::buttonSecondaryBorderColor();
    }

    // Hover
    QString buttonHoverColor() const override 
    {
        return OCC::IonosTheme::buttonSecondaryHoverColor();
    }

    QString buttonHoverBorderColor() const override 
    {
        return OCC::IonosTheme::buttonSecondaryBorderColor();
    }

    // Pressed
    QString buttonPressedColor() const override 
    {
        return OCC::IonosTheme::buttonSecondaryPressedColor();
    }

    QString buttonPressedBorderColor() const override 
    {
        return OCC::IonosTheme::buttonSecondaryBorderColor();
    }

    // Disabled
    QString buttonDisabledColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledColor();
    }

    QString buttonDisabledBorderColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledColor();
    }

    // Focused 
    QString buttonFocusedColor() const override 
    {
        return OCC::IonosTheme::white();
    }

    QString buttonFocusedBorderColor() const override 
    { 
        return OCC::IonosTheme::black();
    }

    // Font
    QString buttonDisabledFontColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledFontColor();
    }

    QString buttonFontColor() const override 
    {
        return OCC::IonosTheme::black();
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
        return OCC::IonosTheme::white();
    }

    QString buttonDefaultBorderColor() const override 
    {
        return OCC::IonosTheme::white();
    }

    // Hover
    QString buttonHoverColor() const override 
    {
        return OCC::IonosTheme::buttonHoveredColor();
    }

    QString buttonHoverBorderColor() const override 
    {
        return OCC::IonosTheme::buttonHoveredColor();
    }

    // Pressed
    QString buttonPressedColor() const override 
    {
        return OCC::IonosTheme::buttonPressedColor();
    }

    QString buttonPressedBorderColor() const override 
    {
        return OCC::IonosTheme::buttonPressedColor();
    }

    // Disabled
    QString buttonDisabledColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledColor();
    }

    QString buttonDisabledBorderColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledColor();
    }

    // Focused 
    QString buttonFocusedColor() const override 
    {
        return OCC::IonosTheme::white();
    }

    QString buttonFocusedBorderColor() const override 
    { 
        return OCC::IonosTheme::black();
    }

    // Font
    QString buttonDisabledFontColor() const override 
    {
        return OCC::IonosTheme::buttonDisabledFontColor();
    }

    QString buttonFontColor() const override 
    {
        return OCC::IonosTheme::black();
    }
};
}

#endif // _BUTTONSTYLE_H
