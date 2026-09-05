TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    common \
    network \
    data \
    services \
    server \
    user_client \
    admin_client \
    unit_tests

common.file = src/common/common.pro
network.file = src/network/network.pro
network.depends = common
data.file = src/data/data.pro
data.depends = common
services.file = src/services/services.pro
services.depends = common data
server.file = apps/server/server.pro
server.depends = common network data services
user_client.file = apps/user_client/user_client.pro
user_client.depends = common network
admin_client.file = apps/admin_client/admin_client.pro
admin_client.depends = common network
unit_tests.file = tests/unit/unit_tests.pro
unit_tests.depends = common network services

