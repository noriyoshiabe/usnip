#include "core/ExportFile.h"
#include "core/JsonImport.h"

#include <QTest>

using namespace usnip;

// Tests for the plain JSON exchange format and the passphrase-protected export file.
class TestExchange : public QObject {
    Q_OBJECT

private slots:
    void parseValidJson();
    void parseRejectsBadInput_data();
    void parseRejectsBadInput();
    void exportRoundTrip();
    void exportWrongPassphraseFails();
    void exportIsNotPlainText();
    void exportRejectsHugeIterationCount();
    void isExportFileChecksHeader();

private:
    static std::vector<PlainSnippet> sample()
    {
        std::vector<PlainSnippet> out;
        out.push_back({QStringLiteral("staging basic auth"), SecureBytes::fromUtf8("user:pass")});
        out.push_back({QStringLiteral("本番 DB"), SecureBytes::fromUtf8("パスワード\n2行目")});
        return out;
    }
    static QString text(const SecureBytes &bytes)
    {
        return QString::fromUtf8(reinterpret_cast<const char *>(bytes.data()),
                                 static_cast<qsizetype>(bytes.size()));
    }
};

void TestExchange::parseValidJson()
{
    const auto parsed =
        jsonimport::parse(R"([{"title": "a", "body": "x"}, {"title": "b", "body": ""}])");
    QVERIFY(parsed);
    QCOMPARE(parsed->size(), std::size_t{2});
    QCOMPARE((*parsed)[0].title, QStringLiteral("a"));
    QCOMPARE(text((*parsed)[0].body), QStringLiteral("x"));
    QVERIFY((*parsed)[1].body.empty());
}

void TestExchange::parseRejectsBadInput_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::newRow("not json") << QByteArray("{");
    QTest::newRow("object") << QByteArray(R"({"title": "a", "body": "x"})");
    QTest::newRow("missing body") << QByteArray(R"([{"title": "a"}])");
    QTest::newRow("empty title") << QByteArray(R"([{"title": "", "body": "x"}])");
    QTest::newRow("number body") << QByteArray(R"([{"title": "a", "body": 1}])");
}

void TestExchange::parseRejectsBadInput()
{
    QFETCH(QByteArray, json);
    QString error;
    QVERIFY(!jsonimport::parse(json, &error));
    QVERIFY(!error.isEmpty());
}

void TestExchange::exportRoundTrip()
{
    const SecureBytes passphrase = SecureBytes::fromUtf8("long passphrase");
    const auto file = exportfile::write(sample(), passphrase, 1000);
    QVERIFY(file);
    const auto imported = exportfile::read(*file, passphrase);
    QVERIFY(imported);
    QCOMPARE(imported->size(), std::size_t{2});
    QCOMPARE((*imported)[1].title, QStringLiteral("本番 DB"));
    QCOMPARE(text((*imported)[1].body), QStringLiteral("パスワード\n2行目"));
}

void TestExchange::exportWrongPassphraseFails()
{
    const auto file = exportfile::write(sample(), SecureBytes::fromUtf8("right"), 1000);
    QVERIFY(file);
    QVERIFY(!exportfile::read(*file, SecureBytes::fromUtf8("wrong")));
}

void TestExchange::exportIsNotPlainText()
{
    const auto file = exportfile::write(sample(), SecureBytes::fromUtf8("p"), 1000);
    QVERIFY(file);
    QVERIFY(file->startsWith("USNX"));
    QVERIFY(!file->contains("staging"));
    QVERIFY(!file->contains("user:pass"));
}

void TestExchange::exportRejectsHugeIterationCount()
{
    const SecureBytes passphrase = SecureBytes::fromUtf8("p");
    auto file = exportfile::write(sample(), passphrase, 1000);
    QVERIFY(file);
    // The iteration count is the last 4 header bytes, right before the sealed payload.
    const qsizetype offset = 4 + 1 + 16;
    for (qsizetype i = 0; i < 4; ++i) {
        (*file)[offset + i] = static_cast<char>(0xff);
    }
    QVERIFY(!exportfile::read(*file, passphrase));
}

void TestExchange::isExportFileChecksHeader()
{
    const auto file = exportfile::write(sample(), SecureBytes::fromUtf8("p"), 1000);
    QVERIFY(file);
    QVERIFY(exportfile::isExportFile(*file));
    QVERIFY(!exportfile::isExportFile(file->left(10)));
    QVERIFY(!exportfile::isExportFile(jsonimport::serialize(sample())));

    QByteArray otherVersion = *file;
    otherVersion[4] = static_cast<char>(exportfile::kFormatVersion + 1);
    QVERIFY(!exportfile::isExportFile(otherVersion));
}

QTEST_GUILESS_MAIN(TestExchange)
#include "tst_exchange.moc"
