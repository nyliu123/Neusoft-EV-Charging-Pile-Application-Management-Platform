// Password hashing utility for admin authentication.
// Uses PBKDF2 with SHA-256, 100000 iterations, and a random salt.

#pragma once

#include <QString>

namespace ev {

struct PasswordHash {
    QByteArray salt;
    QByteArray hash;
    int iterations = 100000;
};

class PasswordHasher final {
public:
    // Generate a random salt and hash the password.
    static PasswordHash hash(const QString &password);

    // Verify a password against a stored hash.
    static bool verify(const QString &password, const PasswordHash &stored);

    // Serialize/deserialize for database storage.
    // Format: "pbkdf2_sha256$<iterations>$<base64_salt>$<base64_hash>"
    static QString serialize(const PasswordHash &ph);
    static bool deserialize(const QString &stored, PasswordHash &out);
};

} // namespace ev
