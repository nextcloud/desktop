#include "sessnackbar.h"
#include "whitelabeltheme.h"
#include "theme.h"
#include <QLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace OCC {

    sesSnackBar::sesSnackBar(QWidget* parent)
        : QFrame(parent)
    {
        setObjectName("sesSnackBar");
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        setContentsMargins(0, 0, 0, 0);

        auto policy = sizePolicy();
        policy.setRetainSizeWhenHidden(false);
        setSizePolicy(policy);
        
        const auto layout = new QHBoxLayout();
        layout->setObjectName("sesSnackBarLayout");
        layout->setContentsMargins(16, 15, 16, 15);
        layout->setSpacing(0);

        m_captionLabel.setObjectName("sesSnackBarCaption");
        m_captionLabel.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        m_captionLabel.setText(m_caption);

        m_messageLabel.setObjectName("sesSnackBarMessage");
        m_messageLabel.setText(m_message);
        m_messageLabel.setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        m_messageLabel.setWordWrap(true);

        m_iconLabel.setObjectName("sesSnackBarIcon");
        m_iconLabel.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_iconLabel.setFixedSize(24, 24);

        layout->addWidget(&m_captionLabel);
        layout->addSpacerItem(new QSpacerItem(8, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
        layout->addWidget(&m_messageLabel);
        layout->addSpacerItem(new QSpacerItem(8, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
        layout->addWidget(&m_iconLabel);
        setLayout(layout);

        errorStyle();
    }

    void sesSnackBar::clearMessage(){
        m_captionLabel.clear();
        m_messageLabel.clear();
    }

    QString sesSnackBar::caption() const { return m_caption; }
    QString sesSnackBar::message() const { return m_message; }

    
    void sesSnackBar::setCaption(QString captionText) {
        if (m_caption != captionText) {
            m_caption = captionText;
            m_captionLabel.setText(m_caption);
            emit captionChanged(m_caption);
        }
    }

    void sesSnackBar::setError(QString errorMessage){
        errorStyle();
        setMessage(errorMessage);
        setCaption(tr("Error"));
        emit errorChanged(m_message);
    }

    void sesSnackBar::setWarning(QString warningMessage){
        warningStyle();
        setMessage(warningMessage);
        setCaption(tr("Warning"));
        emit warningChanged(m_message);
    }

    void sesSnackBar::setSuccess(QString successMessage){
        successStyle();
        setMessage(successMessage);
        setCaption(tr("Success"));
        emit successChanged(m_message);
    }

    void sesSnackBar::setMessage(QString messageText) {
        m_message = messageText;
        m_messageLabel.setText(m_message);
    }

    void sesSnackBar::setWordWrap(bool on)
    {
        m_messageLabel.setWordWrap(on);
    }

    bool sesSnackBar::wordWrap() const
    {
        return m_messageLabel.wordWrap();
    }

    void sesSnackBar::successStyle()
    {
        const auto logoIconFileName = Theme::hidpiFileName(":/client/theme/ses/ses-snackbar-success.svg");
        m_iconLabel.setPixmap(logoIconFileName);

        updateStyleSheet(WLTheme.successBorderColor());
    }

    void sesSnackBar::warningStyle()
    {
        const auto logoIconFileName = Theme::hidpiFileName(":/client/theme/ses/ses-snackbar-warning.svg");
        m_iconLabel.setPixmap(logoIconFileName);

        updateStyleSheet(WLTheme.warningBorderColor());
    }

    void sesSnackBar::errorStyle()
    {
        const auto logoIconFileName = Theme::hidpiFileName(":/client/theme/ses/ses-snackbar-error.svg");
        m_iconLabel.setPixmap(logoIconFileName);

        updateStyleSheet(WLTheme.errorBorderColor());
    }

    void sesSnackBar::updateStyleSheet(QColor frameBorderColor)
    {
        // successColor()/warningColor()/errorColor() and the black() text on top of them were fixed,
        // opaque pastels with no dark variant. Same fix as FolderStatusDelegate: tint the frame with
        // the (already theme-aware) border color at low alpha instead of an opaque fill, and take the
        // text from the widget's own palette instead of a hardcoded black - mirrors the translucent-
        // overlay technique the tray uses for its alert boxes (ErrorBox.qml, trayWindowSyncWarning).
        auto frameBackgroundColor = frameBorderColor;
        frameBackgroundColor.setAlphaF(0.2f);
        const auto textColor = palette().color(QPalette::WindowText);

        QString style = QString::fromLatin1("QFrame {border: 1px solid %1; border-radius: 4px;"
                                "background-color: rgba(%2, %3, %4, %5); color: %6;}"
                                "QLabel {border: 0px none; padding 0px; background-color: transparent; color: %6;}"
                                "QLabel#sesSnackBarCaption {font-weight: bold;}"
                                ).arg(frameBorderColor.name())
                                .arg(frameBackgroundColor.red())
                                .arg(frameBackgroundColor.green())
                                .arg(frameBackgroundColor.blue())
                                .arg(frameBackgroundColor.alpha())
                                .arg(textColor.name());

        setStyleSheet(style);

    }
}