#pragma once

class QByteArray;

QByteArray gZipData(const QByteArray& inputData);

QByteArray unGzipData(const QByteArray& inputData);
