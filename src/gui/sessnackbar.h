#ifndef SESSNACKBAR_H
#define SESSNACKBAR_H

#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionButton>

namespace OCC {

    class sesSnackBar : public QFrame
    {
        Q_OBJECT
        Q_PROPERTY(QString caption READ caption WRITE setCaption NOTIFY captionChanged)
        Q_PROPERTY(QString message READ message)
        Q_PROPERTY(QString error WRITE setError NOTIFY errorChanged)
        Q_PROPERTY(QString warning WRITE setWarning NOTIFY warningChanged)
        Q_PROPERTY(QString success WRITE setSuccess NOTIFY successChanged)
        Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)

    public:
        explicit sesSnackBar(QWidget* parent = nullptr);
        QString caption() const;
        QString message() const;
        bool wordWrap() const;
        void clearMessage();
       
    public slots:
        void setCaption(QString captionText);
        void setError(QString errorMessage);
        void setWarning(QString warningMessage);
        void setSuccess(QString successMessage);

        void setWordWrap(bool on);
    
    signals:
        void captionChanged(QString captionText);
        void errorChanged(QString errorText);
        void warningChanged(QString warningText);
        void successChanged(QString successText);

    private:
        QString m_caption;
        QString m_message;

        QLabel m_messageLabel;
        QLabel m_captionLabel;
        QLabel m_iconLabel;

        void updateStyleSheet(QColor frameBorderColor, QColor frameBackgroundColor, QColor frameColor, QColor labelColor);
        void setMessage(QString messageText);

        void errorStyle();
        void warningStyle();
        void successStyle();

    };
}
#endif // SESSNACKBAR_H
