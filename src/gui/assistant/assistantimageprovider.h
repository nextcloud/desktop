/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

namespace OCC
{
/** @brief Supplies decoded attachment previews without network requests or data URLs. */
class AssistantImageProvider final : public QQuickImageProvider
{
public:
    AssistantImageProvider();
    [[nodiscard]] QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    /** @brief Stores an image under an opaque URL until its attachment releases it. */
    [[nodiscard]] static QString insert(const QImage &image);
    /** @brief Releases the image associated with a previously returned URL. */
    static void remove(const QString &source);

private:
    static QMutex _mutex;
    static QHash<QString, QImage> _images;
};
}
