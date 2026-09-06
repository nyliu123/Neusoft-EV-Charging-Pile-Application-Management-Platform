TEMPLATE = subdirs
CONFIG += ordered

# Keep repository-level resources visible in Qt Creator's Projects tree.
OTHER_FILES += \
    README.md \
    CONTRIBUTING.md \
    LICENSE \
    .editorconfig \
    .gitattributes \
    .gitignore \
    .gitmessage \
    config/common.pri \
    config/app.ini.example \
    docs/02概要设计说明书第5组（最终版）.docx \
    docs/development/architecture.md \
    docs/development/environment.md \
    docs/development/shared-identifiers.md \
    resources/database/migrations/001_core.sql \
    scripts/smoke-test.sh \
    analysis/README.md \
    analysis/feature_engineering.py \
    analysis/train_models.py \
    analysis/predict.py \
    analysis/run_pipeline.py \
    src/adapters/README.md \
    web/screen/README.md

SUBDIRS += \
    common \
    network \
    data \
    services \
    adapters \
    server \
    user_client \
    admin_client \
    unit_tests

common.subdir = src/common
common.target = common
network.subdir = src/network
network.target = network
network.depends = common
data.subdir = src/data
data.target = data
data.depends = common
services.subdir = src/services
services.target = services
services.depends = common data
adapters.subdir = src/adapters
adapters.target = adapters
adapters.depends = common
server.subdir = apps/server
server.target = server
server.depends = common network data services adapters
user_client.subdir = apps/user_client
user_client.target = user_client
user_client.depends = common network
admin_client.subdir = apps/admin_client
admin_client.target = admin_client
admin_client.depends = common network
unit_tests.subdir = tests/unit
unit_tests.target = unit_tests
unit_tests.depends = common network services adapters
