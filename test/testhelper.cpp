#include "testhelper.h"
#include <QJsonObject>
#include <QJsonDocument>

OCC::FolderDefinition folderDefinition(const QString &path)
{
    OCC::FolderDefinition d;
    d.localPath = path;
    d.targetPath = path;
    d.alias = path;
    return d;
}


const QByteArray jsonValueToOccReply(const QJsonValue &jsonValue)
{
    QJsonObject root;
    QJsonObject ocs;
    QJsonObject meta;

    meta.insert("statuscode", 200);

    ocs.insert(QStringLiteral("data"), jsonValue);
    ocs.insert(QStringLiteral("meta"), meta);
    root.insert(QStringLiteral("ocs"), ocs);

    return QJsonDocument(root).toJson();
}
