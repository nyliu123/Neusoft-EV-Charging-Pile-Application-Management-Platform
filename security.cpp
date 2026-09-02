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
    // 教学项目使用独立盐值和多轮 SHA-256，数据库中不保存明文密码。
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
    // 固定遍历完整摘要，避免遇到首个不同字节时提前返回。
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
