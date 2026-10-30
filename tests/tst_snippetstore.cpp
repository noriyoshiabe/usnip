#include "core/SnippetStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace usnip;

class TestSnippetStore : public QObject {
    Q_OBJECT

private slots:
    void addAndReadBody();
    void serializeRoundTrip();
    void titlesAreNotInPlainText();
    void wrongKeyLeavesStoreUnchanged();
    void tamperedFileFails();
    void updateRemoveMove();
    void missingFileIsEmpty();
    void saveFileIsOwnerOnly();

private:
    static SecureBytes master()
    {
        static const SecureBytes key = crypto::randomBytes(crypto::kKeySize);
        return key;
    }
    static SnippetStore newStore()
    {
        return SnippetStore(crypto::deriveKeys(master()));
    }
    static QString bodyText(const SnippetStore &store, std::size_t index)
    {
        const auto body = store.body(index);
        return body ? QString::fromUtf8(reinterpret_cast<const char *>(body->data()),
                                        static_cast<qsizetype>(body->size()))
                    : QString();
    }
};

void TestSnippetStore::addAndReadBody()
{
    SnippetStore store = newStore();
    QVERIFY(store.add(QStringLiteral("prod db"), SecureBytes::fromUtf8("hunter2")));
    QCOMPARE(store.snippets().size(), std::size_t{1});
    QCOMPARE(store.snippets()[0].title, QStringLiteral("prod db"));
    QCOMPARE(bodyText(store, 0), QStringLiteral("hunter2"));
    QVERIFY(!store.body(1));
}

void TestSnippetStore::serializeRoundTrip()
{
    SnippetStore store = newStore();
    QVERIFY(store.add(QStringLiteral("a"), SecureBytes::fromUtf8("alpha")));
    QVERIFY(store.add(QStringLiteral("日本語"), SecureBytes::fromUtf8("ベータ")));
    const auto data = store.serialize();
    QVERIFY(data);

    SnippetStore loaded = newStore();
    QVERIFY(loaded.load(*data));
    QCOMPARE(loaded.snippets().size(), std::size_t{2});
    QCOMPARE(loaded.snippets()[1].title, QStringLiteral("日本語"));
    QCOMPARE(bodyText(loaded, 0), QStringLiteral("alpha"));
    QCOMPARE(bodyText(loaded, 1), QStringLiteral("ベータ"));
}

void TestSnippetStore::titlesAreNotInPlainText()
{
    SnippetStore store = newStore();
    QVERIFY(store.add(QStringLiteral("production-mysql"), SecureBytes::fromUtf8("pw")));
    const auto data = store.serialize();
    QVERIFY(data);
    QVERIFY(data->startsWith("USNP"));
    QVERIFY(!data->contains("production-mysql"));
}

void TestSnippetStore::wrongKeyLeavesStoreUnchanged()
{
    SnippetStore store = newStore();
    QVERIFY(store.add(QStringLiteral("a"), SecureBytes::fromUtf8("alpha")));
    const auto data = store.serialize();
    QVERIFY(data);

    SnippetStore other(crypto::deriveKeys(crypto::randomBytes(crypto::kKeySize)));
    QVERIFY(other.add(QStringLiteral("keep"), SecureBytes::fromUtf8("me")));
    QVERIFY(!other.load(*data));
    QCOMPARE(other.snippets().size(), std::size_t{1});
    QCOMPARE(other.snippets()[0].title, QStringLiteral("keep"));
}

void TestSnippetStore::tamperedFileFails()
{
    SnippetStore store = newStore();
    QVERIFY(store.add(QStringLiteral("a"), SecureBytes::fromUtf8("alpha")));
    auto data = store.serialize();
    QVERIFY(data);

    QByteArray badVersion = *data;
    badVersion[4] = 2;
    QVERIFY(!newStore().load(badVersion));

    QByteArray flipped = *data;
    flipped[flipped.size() / 2] = static_cast<char>(flipped[flipped.size() / 2] ^ 0x01);
    QVERIFY(!newStore().load(flipped));

    QVERIFY(!newStore().load(QByteArray("not a store")));
}

void TestSnippetStore::updateRemoveMove()
{
    SnippetStore store = newStore();
    for (const char *name : {"one", "two", "three"}) {
        QVERIFY(store.add(QString::fromLatin1(name), SecureBytes::fromUtf8(name)));
    }
    const QString idOfTwo = store.snippets()[1].id;

    QVERIFY(store.update(1, QStringLiteral("TWO"), SecureBytes::fromUtf8("2")));
    QCOMPARE(store.snippets()[1].id, idOfTwo);
    QCOMPARE(bodyText(store, 1), QStringLiteral("2"));

    store.move(0, 2);
    QCOMPARE(store.snippets()[0].title, QStringLiteral("TWO"));
    QCOMPARE(store.snippets()[2].title, QStringLiteral("one"));
    QCOMPARE(bodyText(store, 2), QStringLiteral("one"));

    store.remove(0);
    QCOMPARE(store.snippets().size(), std::size_t{2});
    QCOMPARE(store.snippets()[0].title, QStringLiteral("three"));
    QVERIFY(!store.update(5, QStringLiteral("x"), SecureBytes::fromUtf8("x")));
}

void TestSnippetStore::missingFileIsEmpty()
{
    QTemporaryDir dir;
    SnippetStore store = newStore();
    QVERIFY(store.loadFile(dir.filePath(QStringLiteral("missing.bin"))));
    QVERIFY(store.snippets().empty());
}

void TestSnippetStore::saveFileIsOwnerOnly()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("sub/snippets.bin"));
    SnippetStore store = newStore();
    QVERIFY(store.add(QStringLiteral("a"), SecureBytes::fromUtf8("alpha")));
    QVERIFY(store.saveFile(path));

    QCOMPARE(QFile::permissions(path), QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                           QFileDevice::ReadUser | QFileDevice::WriteUser);

    SnippetStore loaded = newStore();
    QVERIFY(loaded.loadFile(path));
    QCOMPARE(bodyText(loaded, 0), QStringLiteral("alpha"));
}

QTEST_GUILESS_MAIN(TestSnippetStore)
#include "tst_snippetstore.moc"
