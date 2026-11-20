/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantimageprovider.h"

#include <QMutexLocker>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace OCC
{
QMutex AssistantImageProvider::_mutex;
QHash<QString, QImage> AssistantImageProvider::_images;

AssistantImageProvider::AssistantImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QString AssistantImageProvider::insert(const QImage &image)
{
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto lock = QMutexLocker{&_mutex};
    _images.insert(id, image);
    return u"image://assistant-attachments/"_s + id;
}

void AssistantImageProvider::remove(const QString &source)
{
    const auto lock = QMutexLocker{&_mutex};
    _images.remove(source.section(u'/', -1));
}

QImage AssistantImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    auto image = QImage{};
    {
        const auto lock = QMutexLocker{&_mutex};
        image = _images.value(id);
    }
    if (size) {
        *size = image.size();
    }
    if (!image.isNull() && requestedSize.isValid()) {
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}
}
