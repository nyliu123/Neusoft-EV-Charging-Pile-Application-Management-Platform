#include "common/password_hasher.h"

#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStringList>

namespace ev {

namespace {

constexpr int kSaltBytes = 16;
constexpr int kHashBytes = 32;
constexpr int kDefaultIterations = 100000;
constexpr const char *kAlgorithmTag = "pbkdf2_sha256";

QByteArray pbkdf2HmacSha256(const QByteArray &password,
                            const QByteArray &salt,
                            int iterations,
                            int dkLen)
{
    // Simple PBKDF2 implementation using QMessageAuthenticationCode.
    // Qt 6.5+ has QPasswordDigestor, but we implement it manually for compatibility.
    QByteArray derived;
    quint32 blockIndex = 1;

    while (derived.size() < dkLen) {
        QByteArray u = salt;
        u.append(static_cast<char>((blockIndex >> 24) & 0xff));
        u.append(static_cast<char>((blockIndex >> 16) & 0xff));
        u.append(static_cast<char>((blockIndex >> 8) & 0xff));
        u.append(static_cast<char>(blockIndex & 0xff));

        QByteArray u1 = QMessageAuthenticationCode::hash(
            u, password, QCryptographicHash::Sha256);
        QByteArray uTotal = u1;

        for (int i = 1; i < iterations; ++i) {
            u1 = QMessageAuthenticationCode::hash(
                u1, password, QCryptographicHash::Sha256);
            for (int j = 0; j < uTotal.size(); ++j) {
                uTotal[j] = uTotal[j] ^ u1[j];
            }
        }

        derived.append(uTotal);
        ++blockIndex;
    }

    return derived.left(dkLen);
}

} // namespace

PasswordHash PasswordHasher::hash(const QString &password)
{
    PasswordHash ph;
    ph.iterations = kDefaultIterations;

    // Generate random salt.
    ph.salt.resize(kSaltBytes);
    QRandomGenerator::system()->generate(ph.salt.begin(), ph.salt.end());

    const QByteArray passwordBytes = password.toUtf8();
    ph.hash = pbkdf2HmacSha256(passwordBytes, ph.salt, ph.iterations, kHashBytes);

    return ph;
}

bool PasswordHasher::verify(const QString &password, const PasswordHash &stored)
{
    const QByteArray passwordBytes = password.toUtf8();
    const QByteArray computed = pbkdf2HmacSha256(
        passwordBytes, stored.salt, stored.iterations, stored.hash.size());

    // Constant-time comparison to prevent timing attacks.
    if (computed.size() != stored.hash.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < computed.size(); ++i) {
        diff |= static_cast<unsigned char>(computed[i] ^ stored.hash[i]);
    }
    return diff == 0;
}

QString PasswordHasher::serialize(const PasswordHash &ph)
{
    return QStringLiteral("%1$%2$%3$%4")
        .arg(QString::fromLatin1(kAlgorithmTag))
        .arg(ph.iterations)
        .arg(QString::fromLatin1(ph.salt.toBase64()))
        .arg(QString::fromLatin1(ph.hash.toBase64()));
}

bool PasswordHasher::deserialize(const QString &stored, PasswordHash &out)
{
    const QStringList parts = stored.split(QLatin1Char('$'));
    if (parts.size() != 4) {
        return false;
    }
    if (parts[0] != QString::fromLatin1(kAlgorithmTag)) {
        return false;
    }

    bool ok = false;
    const int iterations = parts[1].toInt(&ok);
    if (!ok || iterations <= 0) {
        return false;
    }

    out.iterations = iterations;
    out.salt = QByteArray::fromBase64(parts[2].toLatin1());
    out.hash = QByteArray::fromBase64(parts[3].toLatin1());

    return !out.salt.isEmpty() && !out.hash.isEmpty();
}

} // namespace ev
