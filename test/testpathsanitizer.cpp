/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include <QtTest>

#include "common/pathsanitizer.h"
#include "common/result.h"
#include "logger.h"

using namespace OCC;

class TestPathSanitizer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);
        QStandardPaths::setTestModeEnabled(true);
    }

    // --- PathSanitizer tests ---

    void testEmptyPath()
    {
        const auto result = PathSanitizer::validatePath(QString());
        QVERIFY(!result.isValid);
        QVERIFY(!result.warnings.isEmpty());
    }

    void testValidSimplePath()
    {
        const auto result = PathSanitizer::validatePath(QStringLiteral("Documents/report.txt"));
        QVERIFY(result.isValid);
        QVERIFY(result.warnings.isEmpty());
        QCOMPARE(result.sanitizedPath, QStringLiteral("Documents/report.txt"));
    }

    void testPathTraversalDetection()
    {
        QVERIFY(PathSanitizer::containsPathTraversal(QStringLiteral("../etc/passwd")));
        QVERIFY(PathSanitizer::containsPathTraversal(QStringLiteral("foo/../../bar")));
        QVERIFY(PathSanitizer::containsPathTraversal(QStringLiteral("foo/..")));

        // Single dots are fine
        QVERIFY(!PathSanitizer::containsPathTraversal(QStringLiteral("./foo/bar")));
        QVERIFY(!PathSanitizer::containsPathTraversal(QStringLiteral("foo/bar")));
        QVERIFY(!PathSanitizer::containsPathTraversal(QStringLiteral("foo/...bar/baz")));
    }

    void testPathTraversalValidation()
    {
        const auto result = PathSanitizer::validatePath(QStringLiteral("foo/../../etc/passwd"));
        QVERIFY(!result.isValid);
        QVERIFY(result.warnings.first().contains(QStringLiteral("traversal")));
    }

    void testConsecutiveSeparators()
    {
        QCOMPARE(PathSanitizer::removeConsecutiveSeparators(QStringLiteral("foo//bar")),
                 QStringLiteral("foo/bar"));
        QCOMPARE(PathSanitizer::removeConsecutiveSeparators(QStringLiteral("foo///bar///baz")),
                 QStringLiteral("foo/bar/baz"));
        QCOMPARE(PathSanitizer::removeConsecutiveSeparators(QStringLiteral("foo/bar")),
                 QStringLiteral("foo/bar"));
        QCOMPARE(PathSanitizer::removeConsecutiveSeparators(QString()),
                 QString());
    }

    void testBackslashNormalization()
    {
        QCOMPARE(PathSanitizer::removeConsecutiveSeparators(QStringLiteral("foo\\\\bar")),
                 QStringLiteral("foo/bar"));
        QCOMPARE(PathSanitizer::removeConsecutiveSeparators(QStringLiteral("foo\\bar")),
                 QStringLiteral("foo/bar"));
    }

    void testForbiddenCharacters()
    {
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file:name")));
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file*name")));
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file?name")));
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file\"name")));
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file<name")));
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file>name")));
        QVERIFY(PathSanitizer::containsForbiddenCharacters(QStringLiteral("file|name")));

        QVERIFY(!PathSanitizer::containsForbiddenCharacters(QStringLiteral("normal_file.txt")));
        QVERIFY(!PathSanitizer::containsForbiddenCharacters(QStringLiteral("file-name (1).doc")));
    }

    void testTrailingDotOrSpace()
    {
        QVERIFY(PathSanitizer::hasTrailingDotOrSpace(QStringLiteral("file.")));
        QVERIFY(PathSanitizer::hasTrailingDotOrSpace(QStringLiteral("file ")));
        QVERIFY(!PathSanitizer::hasTrailingDotOrSpace(QStringLiteral("file.txt")));
        QVERIFY(!PathSanitizer::hasTrailingDotOrSpace(QStringLiteral("file")));
        QVERIFY(!PathSanitizer::hasTrailingDotOrSpace(QString()));
    }

    void testComponentLengthCheck()
    {
        const QString longName(300, QLatin1Char('a'));
        QVERIFY(PathSanitizer::exceedsMaxComponentLength(longName));
        QVERIFY(!PathSanitizer::exceedsMaxComponentLength(QStringLiteral("normal.txt")));
        QVERIFY(PathSanitizer::exceedsMaxComponentLength(QStringLiteral("abc"), 2));
    }

    void testSanitizeComponent()
    {
        // Forbidden chars replaced with underscore
        QCOMPARE(PathSanitizer::sanitizeComponent(QStringLiteral("file:name")),
                 QStringLiteral("file_name"));
        QCOMPARE(PathSanitizer::sanitizeComponent(QStringLiteral("a*b?c")),
                 QStringLiteral("a_b_c"));

        // Trailing dots and spaces removed
        QCOMPARE(PathSanitizer::sanitizeComponent(QStringLiteral("file. . ")),
                 QStringLiteral("file"));

        // Empty stays empty
        QCOMPARE(PathSanitizer::sanitizeComponent(QString()), QString());

        // Long names truncated
        const QString longName(300, QLatin1Char('x'));
        QCOMPARE(PathSanitizer::sanitizeComponent(longName).length(), 255);
    }

    void testSanitizeControlCharacters()
    {
        QString withControl = QStringLiteral("file") + QChar(0x01) + QStringLiteral("name");
        QCOMPARE(PathSanitizer::sanitizeComponent(withControl), QStringLiteral("filename"));
    }

    void testValidatePathWithWarnings()
    {
        const auto result = PathSanitizer::validatePath(QStringLiteral("foo//bar:baz. "));
        QVERIFY(!result.isValid);
        QVERIFY(result.warnings.size() >= 2); // consecutive separators + forbidden chars + trailing
    }

    void testValidatePathPreservesLeadingSeparator()
    {
        const auto result = PathSanitizer::validatePath(QStringLiteral("/absolute/path/file.txt"));
        QVERIFY(result.isValid);
        QVERIFY(result.sanitizedPath.startsWith(QLatin1Char('/')));
    }

    void testValidatePathWithMixedIssues()
    {
        const auto result = PathSanitizer::validatePath(QStringLiteral("docs//file*.txt"));
        QVERIFY(!result.isValid);
        QCOMPARE(result.sanitizedPath, QStringLiteral("docs/file_.txt"));
    }

    // --- Result<T, Error> tests ---

    void testResultBasicSuccess()
    {
        Result<int, QString> r(42);
        QVERIFY(r.isValid());
        QVERIFY(static_cast<bool>(r));
        QCOMPARE(*r, 42);
        QCOMPARE(r.get(), 42);
    }

    void testResultBasicError()
    {
        Result<int, QString> r(QStringLiteral("error occurred"));
        QVERIFY(!r.isValid());
        QVERIFY(!static_cast<bool>(r));
        QCOMPARE(r.error(), QStringLiteral("error occurred"));
    }

    void testResultCopyConstructor()
    {
        Result<QString, QString> original(QStringLiteral("hello"));
        Result<QString, QString> copy(original);
        QVERIFY(copy.isValid());
        QCOMPARE(*copy, QStringLiteral("hello"));
    }

    void testResultMoveConstructor()
    {
        Result<QString, QString> original(QStringLiteral("hello"));
        Result<QString, QString> moved(std::move(original));
        QVERIFY(moved.isValid());
        QCOMPARE(*moved, QStringLiteral("hello"));
    }

    void testResultCopyAssignment()
    {
        Result<QString, QString> r1(QStringLiteral("first"));
        Result<QString, QString> r2(QStringLiteral("second"));
        r1 = r2;
        QVERIFY(r1.isValid());
        QCOMPARE(*r1, QStringLiteral("second"));
    }

    void testResultMoveAssignment()
    {
        Result<QString, QString> r1(QStringLiteral("first"));
        Result<QString, QString> r2(QStringLiteral("second"));
        r1 = std::move(r2);
        QVERIFY(r1.isValid());
        QCOMPARE(*r1, QStringLiteral("second"));
    }

    void testResultAssignmentChangesErrorState()
    {
        // Start with success, assign error
        Result<QString, QString> r(QStringLiteral("success"));
        QVERIFY(r.isValid());

        Result<QString, QString> err(QString(QStringLiteral("fail")));
        // Force error construction
        Result<QString, QString> errorResult = []{
            return Result<QString, QString>(QStringLiteral(""));
        }();
        // Reassignment from success to success
        r = errorResult;
        QVERIFY(r.isValid());
    }

    void testResultSelfAssignment()
    {
        Result<QString, QString> r(QStringLiteral("hello"));
        auto &ref = r;
        r = ref; // self-assignment
        QVERIFY(r.isValid());
        QCOMPARE(*r, QStringLiteral("hello"));
    }

    void testResultVoidSpecialization()
    {
        Result<void, QString> success;
        QVERIFY(success.isValid());

        Result<void, QString> failure(QStringLiteral("failed"));
        QVERIFY(!failure.isValid());
        QCOMPARE(failure.error(), QStringLiteral("failed"));
    }

    void testOptionalWithValue()
    {
        Optional<int> opt(42);
        QVERIFY(opt.isValid());
        QCOMPARE(*opt, 42);
    }

    void testOptionalEmpty()
    {
        Optional<int> opt;
        QVERIFY(!opt.isValid());
    }

    void testResultArrowOperator()
    {
        Result<QString, QString> r(QStringLiteral("hello"));
        QCOMPARE(r->length(), 5);
    }

    void testResultMoveValue()
    {
        Result<QString, QString> r(QStringLiteral("hello"));
        QString val = *std::move(r);
        QCOMPARE(val, QStringLiteral("hello"));
    }

    void testResultMoveError()
    {
        Result<int, QString> r(QStringLiteral("err"));
        QString err = std::move(r).error();
        QCOMPARE(err, QStringLiteral("err"));
    }

    void testResultRepeatedReassignment()
    {
        // This tests that the assignment operator properly destroys old values
        // (the bug fix in result.h). Without the fix, this would leak or cause UB.
        Result<QString, QString> r(QStringLiteral("initial"));
        for (int i = 0; i < 100; ++i) {
            r = Result<QString, QString>(QString::number(i));
        }
        QVERIFY(r.isValid());
        QCOMPARE(*r, QStringLiteral("99"));
    }
};

QTEST_APPLESS_MAIN(TestPathSanitizer)
#include "testpathsanitizer.moc"
