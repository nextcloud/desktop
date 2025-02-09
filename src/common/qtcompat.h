
#pragma once

#include <QtGlobal>
#include <QLocale>
#include <QRegularExpression>
#include <QTimeZone>

#include <type_traits>

//-----------------------------------------------------------------------------

template<unsigned major, unsigned minor, class T = void>
struct qt_atleast : public std::enable_if<QT_VERSION>=QT_VERSION_CHECK(major, minor, 0), T>
{
};

template<unsigned major, unsigned minor, class T = void>
struct qt_before : public std::enable_if<QT_VERSION<QT_VERSION_CHECK(major, minor, 0), T>
{
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

inline QString systemLocaleName()
{
#if QT_VERSION>=QT_VERSION_CHECK(6, 8, 0)
    return QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore).first();
#else
    return QLocale::system().name();
#endif
}

//-----------------------------------------------------------------------------

template <typename T>
inline QRegularExpressionMatch
matchRegularExpression(const QRegularExpression& re, const T& s)
{
#if QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
    return re.matchView(s);
#else
    return re.match(s);
#endif
}

//-----------------------------------------------------------------------------

#if QT_VERSION<QT_VERSION_CHECK(6, 5, 0)
template <typename Char, typename... Args>
inline QDebug& operator<<(QDebug& d, const std::basic_string<Char, Args...>& s)
{
    return d << s.c_str();
}
#endif

//-----------------------------------------------------------------------------

#if QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
const auto QTimeZoneUTC = QTimeZone::UTC;
#else
const auto QTimeZoneUTC = Qt::UTC;
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef QT_WIDGETS_LIB

//-----------------------------------------------------------------------------

#include <QCheckBox>

//-----------------------------------------------------------------------------

template <typename Functor>
inline QMetaObject::Connection connectCheckBoxStateChanged(
    const QCheckBox* sender, const QObject* context, Functor functor)
{
    return QObject::connect(sender,
#if QT_VERSION>=QT_VERSION_CHECK(6, 8, 0)
                            &QCheckBox::checkStateChanged,
#else
                            &QCheckBox::stateChanged,
#endif
                            context, functor);
}

//-----------------------------------------------------------------------------

#endif // QT_WIDGETS_LIB

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef QT_WEBSOCKETS_LIB

//-----------------------------------------------------------------------------

#include <QWebSocket>

//-----------------------------------------------------------------------------

#if QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
#define QWebSocketErrorOccurred &QWebSocket::errorOccurred
#else
#define QWebSocketErrorOccurred &QWebSocket::error
#endif

//-----------------------------------------------------------------------------

#endif // QT_WEBSOCKETS_LIB

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef QT_XML_LIB

//-----------------------------------------------------------------------------

#include <QDomDocument>

//-----------------------------------------------------------------------------

#if QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)

inline QDomDocument::ParseResult
QDomDocumentSetContentsUseNamespaceProcessing(QDomDocument& doc, QIODevice* device)
{
    return doc.setContent(device, QDomDocument::ParseOption::UseNamespaceProcessing);
}

#else // QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)

struct QDomDocumentParseResult {
    bool retval = false;

    QString errorMessage;

    int errorLine = -1;

    int errorColumn = -1;

    inline operator bool() const {
        return retval;
    }
};

inline QDomDocumentParseResult
QDomDocumentSetContentsUseNamespaceProcessing(QDomDocument& doc, QIODevice* device)
{
    QDomDocumentParseResult result;
    result.retval = doc.setContent(device, true, &result.errorMessage,
                                   &result.errorLine, &result.errorColumn);
    return result;
}

#endif // QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)

//-----------------------------------------------------------------------------

#endif // QT_XML_LIB

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef QT_GUI_LIB

//-----------------------------------------------------------------------------

#include <QGuiApplication>
#include <QStyleHints>

//-----------------------------------------------------------------------------

inline double getColorDarkness(const QColor &color)
{
    // account for different sensitivity of the human eye to certain colors
    const double threshold = 1.0 - (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0;
    return threshold;
}

//-----------------------------------------------------------------------------

inline bool isDarkColor(const QColor& color)
{
    return getColorDarkness(color) > 0.5;
}

//-----------------------------------------------------------------------------

template <class Target, typename Functor>
inline void connectQStyleHintsColorSchemeChanged(QGuiApplication* app,
                                                 const Target* context,
                                                 Functor functor)
{
#if QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(app->styleHints(), &QStyleHints::colorSchemeChanged,
                     context, functor, Qt::UniqueConnection);
#else
    QObject::connect(app, &QGuiApplication::paletteChanged, context, functor);
#endif
}

//-----------------------------------------------------------------------------

inline bool isDarkMode() {
#if QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
    switch (qGuiApp->styleHints()->colorScheme())
    {
      case Qt::ColorScheme::Dark:
        return true;
      case Qt::ColorScheme::Light:
        return false;
      case Qt::ColorScheme::Unknown:
        return isDarkColor(QGuiApplication::palette().window().color());
    }

    return false;
#else // QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
    return isDarkColor(QGuiApplication::palette().window().color());
#endif // QT_VERSION>=QT_VERSION_CHECK(6, 5, 0)
}

//-----------------------------------------------------------------------------

#endif // QT_GUI_LIB

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
