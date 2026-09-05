-- Seed default admin account for initial setup.
-- Password hashed with PBKDF2 (SHA-256, 100000 iterations, 16-byte salt).
-- Default credentials: admin / admin123
-- IMPORTANT: Change the default password after first login in production.

INSERT OR IGNORE INTO admins (username, password)
VALUES ('admin', 'pbkdf2_sha256$100000$c2VlZF9hZG1pbl9zYWx0XzAwMQ==$H5O6QA79pEMZHKSK/zktsntFmo0m2SXLNPgbKIyfkYM=');
