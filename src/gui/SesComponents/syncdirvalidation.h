#ifndef SYNCDIRVALIDATION_H
#define SYNCDIRVALIDATION_H

#include <QDir>

class SyncDirValidator {
public:
    SyncDirValidator(const QString &path) : _path(path) {}

    bool isValidDir();
    QString message();

private:
    QString appDataPath();
    QString _path;
};


#endif // SYNCDIRVALIDATION_H