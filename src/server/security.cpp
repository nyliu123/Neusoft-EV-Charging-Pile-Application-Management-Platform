#include "security.h"

#include <QCryptographicHash>
#include <QUuid>

namespace evcs::server::security {

QString createSalt()
{
    return QString::fromLatin1(
        QUuid::createUuid().toRfc4122().toBase64(QByteArray::OmitTrailingEquals));
}

QString hashPassword(const QString &password, const QString &salt)
{
    const QByteArray passwordBytes = password.toUtf8();
    const QByteArray saltBytes = salt.toUtf8();
    QByteArray digest = saltBytes + passwordBytes;
    for (int round = 0; round < 50000; ++round) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(digest);
        hash.addData(passwordBytes);
        hash.addData(saltBytes);
        digest = hash.result();
    }
    return QString::fromLatin1(digest.toHex());
}

bool verifyPassword(const QString &password,
                    const QString &salt,
                    const QString &expectedHash)
{
    const QByteArray actual = hashPassword(password, salt).toLatin1();
    const QByteArray expected = expectedHash.toLatin1();
    if (actual.size() != expected.size()) {
        return false;
    }

    uchar difference = 0;
    for (qsizetype index = 0; index < actual.size(); ++index) {
        difference |= static_cast<uchar>(actual.at(index) ^ expected.at(index));
    }
    return difference == 0;
}

} // namespace evcs::server::security
