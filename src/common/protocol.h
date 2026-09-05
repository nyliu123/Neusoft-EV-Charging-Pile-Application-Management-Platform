#pragma once

#include <QtGlobal>

namespace ev {

inline constexpr quint32 ProtocolVersion = 1;
inline constexpr qsizetype FrameHeaderBytes = 8;
inline constexpr quint32 MaxPayloadBytes = 10U * 1024U * 1024U;

enum class MessageType : quint32 {
    LoginRequest = 0x01,
    LoginResponse = 0x02,
    HealthRequest = 0x03,
    HealthResponse = 0x04,
    SessionHeartbeatRequest = 0x05,
    SessionHeartbeatResponse = 0x06,
    LogoutRequest = 0x07,
    LogoutResponse = 0x08,
    UserRequest = 0x10,
    UserResponse = 0x11,
    StationRequest = 0x20,
    StationResponse = 0x21,
    ChargeRequest = 0x30,
    ChargeResponse = 0x31,
    ChargeUpdate = 0x32,
    MembershipRequest = 0x40,
    MembershipResponse = 0x41,
    ConsultRequest = 0x50,
    ConsultResponse = 0x51,
    AdminQuery = 0x60,
    AdminAction = 0x61,
    AdminResponse = 0x62,
    ErrorResponse = 0x7f
};

} // namespace ev
