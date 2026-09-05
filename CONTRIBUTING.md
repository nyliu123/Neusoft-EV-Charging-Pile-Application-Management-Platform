# 贡献指南

## 分支与合并

1. 从最新 `main` 创建功能分支，禁止直接提交到 `main`。
2. 分支建议使用 `feature/<模块>-<说明>`、`fix/<模块>-<说明>`、`test/<模块>-<说明>` 或 `docs/<说明>`。
3. 提交前同步最新主干，解决冲突后完成全量编译和相关回归测试。
4. 合并请求必须说明需求编号、业务影响、测试方法以及数据库或协议兼容性。
5. 至少一名非作者成员审核后方可合并；涉及共享协议、DDL 或公共头文件时须由对应模块负责人审核。

可选地启用仓库提交模板：

```bash
git config commit.template .gitmessage
git config pull.rebase true
git config fetch.prune true
```

这些设置只作用于当前仓库，不应把个人身份、凭据或代理配置写入版本控制。

## 提交要求

- 提交标题采用 `<type>(<scope>): <subject>`，例如 `feat(network): add socket gateway`。
- 每个提交保持单一目的，不提交构建产物、数据库、日志、密钥和个人 IDE 配置。
- 共享标识符只在一个公共头文件中定义，禁止在各分支复制枚举、常量或结构体。
- 数据库迁移只能新增有序脚本；已经进入主干的迁移不得原地改写。
- 金额在业务层使用整数分，协议和数据库变更需同时更新测试与文档。

## 合并前检查

```bash
mkdir -p build
cd build
qmake6 ../ev-charging-platform.pro
make -j"$(nproc)"
./bin/ev_unit_tests
```

同时检查用户端、管理端和服务端能启动；新增写业务必须覆盖失败、重复提交和并发冲突场景。

