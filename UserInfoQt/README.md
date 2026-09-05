# UserInfoQt（qmake，修正版 v2）

本版本严格按照用户信息中任务 UML-014 ~ UML-017 的职责进行独立开发验证。

## 本次修正

1. 删除底部“消息 / 我的北理 / 通讯录 / 我的 / 更多”导航栏。
2. 删除头像旁的“北京理工大学 · 个人账户”。
3. 页面首屏数据只从 `UserGlobalState::userInfo()` 读取。
4. 不再在 UI 或 `UserService` 构造函数中写死“禹晨、北京理工大学、128.50”等虚构用户初始数据。
5. 因本模块没有 UML-013 登录模块，独立运行时显示“未登录用户”状态；合入总项目后由 UML-013 登录成功流程写入全局状态。
6. 修改头像、昵称、充值成功后，同步更新全局 `user_info`。
7. 刷新从模拟用户服务获取最新数据，再更新全局状态。

## Qt Creator

打开 `UserInfoQt.pro`，选择 Desktop Qt Kit，然后：

- 构建 -> 清理项目
- 构建 -> 运行 qmake
- 构建 -> 构建项目
- 运行

## 合入总项目

实际总项目中，UML-013 登录成功后应执行类似：

```cpp
UserGlobalState::instance().setUserInfo(loginUserInfo);
```

随后打开个人信息页即可首屏读取缓存的 `user_info`，不需要再次查询数据库。

本独立版本中的 `UserService` 只是模拟服务层，UI 不直接访问数据库。
