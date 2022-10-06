#include "shellextensionutils.h"
#include <QJsonDocument>
#include <QLoggingCategory>

namespace VfsShellExtensions {

Q_LOGGING_CATEGORY(lcShellExtensionUtils, "nextcloud.gui.shellextensionutils", QtInfoMsg)

QString VfsShellExtensions::serverNameForApplicationName(const QString &applicationName)
{
    return applicationName + QStringLiteral(":VfsShellExtensionsServer");
}

QString VfsShellExtensions::serverNameForApplicationNameDefault()
{
    return serverNameForApplicationName(APPLICATION_NAME);
}
namespace Protocol {
    QByteArray createJsonMessage(const QVariantMap &message)
    {
        QVariantMap messageCopy = message;
        messageCopy[QStringLiteral("version")] = Version;
        return QJsonDocument::fromVariant((messageCopy)).toJson(QJsonDocument::Compact);
    }

    bool validateProtocolVersion(const QVariantMap &message)
    {
        const auto valid = message.value(QStringLiteral("version")) == Version;
        if (!valid) {
            qCWarning(lcShellExtensionUtils) << "Invalid shell extensions IPC protocol: " << message.value(QStringLiteral("version")) << " vs " << Version;
        }
        return valid;
    }
}
}
