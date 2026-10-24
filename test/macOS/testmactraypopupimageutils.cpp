/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/trayaccountpopup/trayaccountpopupimageutils.h"

#include <QTemporaryDir>
#include <QtTest>

#include <ranges>
#include <tuple>
#include <vector>

#import <objc/runtime.h>

using namespace Qt::StringLiterals;
namespace ImageUtils = OCC::Mac::TrayPopupImageUtils;

class TestMacTrayPopupImageUtils : public QObject
{
    Q_OBJECT

private:
    static constexpr auto imageWidth = 11;
    static constexpr auto imageHeight = 7;

    NSHashTable<NSImage *> *_images = nil;
    NSHashTable<NSBitmapImageRep *> *_bitmaps = nil;
    std::vector<std::tuple<Method, IMP, IMP>> _hooks;
    bool _missingBitmapData = false;

    bool installHook(Method method, id implementationBlock)
    {
        if (!method) {
            return false;
        }
        const auto replacement = imp_implementationWithBlock(implementationBlock);
        if (!replacement) {
            return false;
        }
        _hooks.emplace_back(method, method_getImplementation(method), replacement);
        method_setImplementation(method, replacement);
        return true;
    }

    static bool matchesBitmap(NSBitmapImageRep *bitmap)
    {
        return bitmap.pixelsWide == imageWidth && bitmap.pixelsHigh == imageHeight;
    }

    static QImage sourceImage(QImage::Format format = QImage::Format_RGBA8888, qreal scale = 1.0)
    {
        auto source = QImage(imageWidth, imageHeight, format);
        source.fill(QColor(90, 120, 210, 128));
        source.setPixelColor(1, 2, QColor(255, 64, 32, 192));
        source.setPixelColor(imageWidth - 1, imageHeight - 1, QColor(12, 34, 56, 255));
        source.setDevicePixelRatio(scale);
        return source;
    }

    static QByteArray rgbaBytes(const QImage &source)
    {
        const auto rgba = source.convertToFormat(QImage::Format_RGBA8888);
        return QByteArray(reinterpret_cast<const char *>(rgba.constBits()), rgba.sizeInBytes());
    }

    void verifyPixels(NSImage *image, const QByteArray &expectedBytes, qreal scale)
    {
        QVERIFY(image);
        QCOMPARE(image.size.width, imageWidth / scale);
        QCOMPARE(image.size.height, imageHeight / scale);
        QCOMPARE(image.representations.count, 1);
        const auto bitmap = (NSBitmapImageRep *)image.representations.firstObject;
        QVERIFY([bitmap isKindOfClass:NSBitmapImageRep.class]);
        QCOMPARE(bitmap.pixelsWide, imageWidth);
        QCOMPARE(bitmap.pixelsHigh, imageHeight);
        QCOMPARE(bitmap.bitsPerSample, 8);
        QCOMPARE(bitmap.samplesPerPixel, 4);
        QVERIFY(bitmap.hasAlpha);
        QCOMPARE(bitmap.size.width, image.size.width);
        QCOMPARE(bitmap.size.height, image.size.height);
        QVERIFY(bitmap.bitmapData);
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(bitmap.bitmapData), bitmap.bytesPerRow * bitmap.pixelsHigh), expectedBytes);
    }

    void verifyLiveObjects(int images, int bitmaps)
    {
        @autoreleasepool {
            QCOMPARE(_images.allObjects.count, images);
            QCOMPARE(_bitmaps.allObjects.count, bitmaps);
        }
    }

private Q_SLOTS:
    void init()
    {
        @autoreleasepool {
            [NSApplication sharedApplication];
            _missingBitmapData = false;
            _images = [NSHashTable weakObjectsHashTable];
            _bitmaps = [NSHashTable weakObjectsHashTable];

            const auto probe = ImageUtils::nsImageFromQImage(sourceImage());
            QVERIFY(probe);
            const auto bitmapClass = object_getClass(probe.representations.firstObject);
            const auto dataMethod = class_getInstanceMethod(bitmapClass, @selector(bitmapData));
            QVERIFY(dataMethod);
            const auto dataOriginal = reinterpret_cast<unsigned char *(*)(id, SEL)>(method_getImplementation(dataMethod));
            QVERIFY(installHook(dataMethod, ^unsigned char *(NSBitmapImageRep *bitmap) {
                if (matchesBitmap(bitmap)) {
                    [_bitmaps addObject:bitmap];
                    if (_missingBitmapData) {
                        return nullptr;
                    }
                }
                return dataOriginal(bitmap, @selector(bitmapData));
            }));

            const auto addMethod = class_getInstanceMethod(NSImage.class, @selector(addRepresentation:));
            QVERIFY(addMethod);
            const auto addOriginal = reinterpret_cast<void (*)(id, SEL, NSImageRep *)>(method_getImplementation(addMethod));
            QVERIFY(installHook(addMethod, ^(NSImage *image, NSImageRep *representation) {
                if ([representation isKindOfClass:NSBitmapImageRep.class] && matchesBitmap((NSBitmapImageRep *)representation)) {
                    [_images addObject:image];
                }
                addOriginal(image, @selector(addRepresentation:), representation);
            }));
        }
    }

    void cleanup()
    {
        @autoreleasepool {
            for (const auto &[method, original, replacement] : std::views::reverse(_hooks)) {
                method_setImplementation(method, original);
                imp_removeBlock(replacement);
            }
            _hooks.clear();
            _images = nil;
            _bitmaps = nil;
        }
    }

    void copiesPixelsAndPreservesLogicalSize_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<qreal>("scale");
        QTest::newRow("rgba") << int(QImage::Format_RGBA8888) << qreal(1.0);
        QTest::newRow("retina") << int(QImage::Format_RGBA8888) << qreal(2.0);
        QTest::newRow("fractional-scale") << int(QImage::Format_RGBA8888) << qreal(1.5);
        QTest::newRow("argb") << int(QImage::Format_ARGB32) << qreal(1.0);
        QTest::newRow("premultiplied") << int(QImage::Format_ARGB32_Premultiplied) << qreal(1.0);
        QTest::newRow("rgb") << int(QImage::Format_RGB888) << qreal(1.0);
        QTest::newRow("grayscale") << int(QImage::Format_Grayscale8) << qreal(1.0);
    }

    void copiesPixelsAndPreservesLogicalSize()
    {
        QFETCH(int, format);
        QFETCH(qreal, scale);
        auto source = sourceImage(static_cast<QImage::Format>(format), scale);
        const auto expectedBytes = rgbaBytes(source);
        NSImage *__attribute__((objc_precise_lifetime)) image = nil;
        @autoreleasepool {
            image = ImageUtils::nsImageFromQImage(source);
        }
        source = {};

        @autoreleasepool {
            verifyPixels(image, expectedBytes, scale);
            verifyLiveObjects(1, 1);
        }

        @autoreleasepool {
            image = nil;
        }
        verifyLiveObjects(0, 0);
    }

    void nativeImageViewOwnsImageAcrossAutoreleasePool()
    {
        const auto source = sourceImage();
        const auto expectedBytes = rgbaBytes(source);
        const auto view = [[NSImageView alloc] initWithFrame:NSZeroRect];
        @autoreleasepool {
            view.image = ImageUtils::nsImageFromQImage(source);
        }

        @autoreleasepool {
            verifyPixels(view.image, expectedBytes, 1.0);
            verifyLiveObjects(1, 1);
        }

        @autoreleasepool {
            view.image = nil;
        }
        verifyLiveObjects(0, 0);
    }

    void unownedImageIsReleased()
    {
        @autoreleasepool {
            ImageUtils::nsImageFromQImage(sourceImage());
        }
        verifyLiveObjects(0, 0);
    }

    void nullImageReturnsNil()
    {
        @autoreleasepool {
            QVERIFY(!ImageUtils::nsImageFromQImage({}));
        }
        verifyLiveObjects(0, 0);
    }

    void missingBitmapBufferReleasesRepresentation()
    {
        _missingBitmapData = true;
        @autoreleasepool {
            QVERIFY(!ImageUtils::nsImageFromQImage(sourceImage()));
        }
        verifyLiveObjects(0, 0);
    }

    void loadsLocalUrlIntoNativeImage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto source = sourceImage();
        const auto expectedBytes = rgbaBytes(source);
        const auto path = directory.filePath(u"icon.png"_s);
        QVERIFY(source.save(path));
        NSImage *__attribute__((objc_precise_lifetime)) image = nil;
        @autoreleasepool {
            image = ImageUtils::nsImageFromQUrl(QUrl::fromLocalFile(path));
        }

        @autoreleasepool {
            verifyPixels(image, expectedBytes, 1.0);
        }
        @autoreleasepool {
            image = nil;
        }
        verifyLiveObjects(0, 0);
    }

    void invalidImageUrlsReturnNil()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto invalidPath = directory.filePath(u"invalid.png"_s);
        QFile invalidFile(invalidPath);
        QVERIFY(invalidFile.open(QIODevice::WriteOnly));
        QVERIFY(invalidFile.write("not an image") > 0);
        invalidFile.close();

        @autoreleasepool {
            QVERIFY(!ImageUtils::nsImageFromQUrl({}));
            QVERIFY(!ImageUtils::nsImageFromQUrl(QUrl::fromLocalFile(directory.filePath(u"missing.png"_s))));
            QVERIFY(!ImageUtils::nsImageFromQUrl(QUrl::fromLocalFile(invalidPath)));
        }
        verifyLiveObjects(0, 0);
    }
};

QTEST_MAIN(TestMacTrayPopupImageUtils)

#include "testmactraypopupimageutils.moc"
