# 共享标识符清单

本清单是跨分支公共名称的唯一登记入口。修改名称或签名时，必须在同一次合并中更新所有调用方与本清单。

| 标识符 | 所属域 | 唯一定义位置 | 用途 |
|---|---|---|---|
| `ev::ProtocolVersion` | 通信 | `src/common/protocol.h` | 当前协议版本 |
| `ev::MaxPayloadBytes` | 通信 | `src/common/protocol.h` | 单帧正文最大 10 MiB |
| `ev::MessageType` | 通信 | `src/common/protocol.h` | TCP 消息类型编号 |
| `ev::ErrorCode` | 公共 | `src/common/error_code.h` | 统一业务错误码 |
| `ev::Result<T>` | 公共 | `src/common/result.h` | 服务统一返回结构 |
| `ev::Frame` | 通信 | `src/network/frame_codec.h` | 已解析消息帧 |
| `ev::FrameCodec` | 通信 | `src/network/frame_codec.h` | 8 字节头加 JSON 正文编解码 |
| `ev::FeeCalculator` | 充电 | `src/services/fee_calculator.h` | 按订单快照计算整数分费用 |
| `ev::DatabaseManager` | 数据 | `src/data/database_manager.h` | 每线程 SQLite 连接与迁移入口 |

新增消息类型必须显式分配未使用编号，并同步更新客户端、服务端与协议测试；禁止依据枚举顺序隐式生成线上编号。

