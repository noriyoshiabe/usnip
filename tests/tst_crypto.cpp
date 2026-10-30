#include "core/Crypto.h"

#include <QTest>

using namespace usnip;
using namespace usnip::crypto;

class TestCrypto : public QObject {
    Q_OBJECT

private slots:
    void roundTrip();
    void emptyPlaintext();
    void sealIsRandomized();
    void wrongKeyFails();
    void wrongAadFails();
    void tamperedBytesFail();
    void truncatedFails();
    void derivedKeysDiffer();
    void passphraseDerivationIsDeterministic();

private:
    static Keys newKeys()
    {
        return deriveKeys(randomBytes(kKeySize));
    }
};

void TestCrypto::roundTrip()
{
    const Keys keys = newKeys();
    const SecureBytes plaintext = SecureBytes::fromUtf8("mysql password: s3cr3t!");
    const auto sealed = seal(keys, asBytes("ctx"), plaintext.view());
    QVERIFY(sealed);
    const auto opened = open(keys, asBytes("ctx"), *sealed);
    QVERIFY(opened);
    QVERIFY(*opened == plaintext);
}

void TestCrypto::emptyPlaintext()
{
    const Keys keys = newKeys();
    const auto sealed = seal(keys, {}, {});
    QVERIFY(sealed);
    QCOMPARE(sealed->size(), kIvSize + kBlockSize + kMacSize);
    const auto opened = open(keys, {}, *sealed);
    QVERIFY(opened);
    QVERIFY(opened->empty());
}

void TestCrypto::sealIsRandomized()
{
    const Keys keys = newKeys();
    const SecureBytes plaintext = SecureBytes::fromUtf8("same input");
    QVERIFY(*seal(keys, {}, plaintext.view()) != *seal(keys, {}, plaintext.view()));
}

void TestCrypto::wrongKeyFails()
{
    const auto sealed = seal(newKeys(), {}, asBytes("secret"));
    QVERIFY(sealed);
    QVERIFY(!open(newKeys(), {}, *sealed));
}

void TestCrypto::wrongAadFails()
{
    const Keys keys = newKeys();
    const auto sealed = seal(keys, asBytes("body:a"), asBytes("secret"));
    QVERIFY(sealed);
    QVERIFY(!open(keys, asBytes("body:b"), *sealed));
}

void TestCrypto::tamperedBytesFail()
{
    const Keys keys = newKeys();
    const auto sealed = seal(keys, {}, asBytes("0123456789abcdef0123"));
    QVERIFY(sealed);
    // Flip one bit in the IV, the ciphertext and the MAC in turn.
    for (std::size_t i : {std::size_t{0}, kIvSize + 1, sealed->size() - 1}) {
        Bytes copy = *sealed;
        copy[i] ^= 0x01;
        QVERIFY2(!open(keys, {}, copy), qPrintable(QStringLiteral("offset %1").arg(i)));
    }
}

void TestCrypto::truncatedFails()
{
    const Keys keys = newKeys();
    auto sealed = seal(keys, {}, asBytes("secret"));
    QVERIFY(sealed);
    sealed->pop_back();
    QVERIFY(!open(keys, {}, *sealed));
    QVERIFY(!open(keys, {}, Bytes(kIvSize + kMacSize)));
}

void TestCrypto::derivedKeysDiffer()
{
    const Keys keys = newKeys();
    QCOMPARE(keys.enc.size(), kKeySize);
    QCOMPARE(keys.mac.size(), kKeySize);
    QVERIFY(!(keys.enc == keys.mac));
}

void TestCrypto::passphraseDerivationIsDeterministic()
{
    const SecureBytes passphrase = SecureBytes::fromUtf8("correct horse");
    const SecureBytes salt = randomBytes(kSaltSize);
    const SecureBytes a = deriveFromPassphrase(passphrase, salt.view(), 1000);
    const SecureBytes b = deriveFromPassphrase(passphrase, salt.view(), 1000);
    const SecureBytes c = deriveFromPassphrase(passphrase, salt.view(), 1001);
    QCOMPARE(a.size(), kKeySize);
    QVERIFY(a == b);
    QVERIFY(!(a == c));
}

QTEST_GUILESS_MAIN(TestCrypto)
#include "tst_crypto.moc"
