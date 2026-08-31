#!/usr/bin/env python3
"""Local HTTP bridge from the ECharts dashboard to the EVCS TCP/JSON service."""

from __future__ import annotations

import argparse
import json
import os
import socket
import struct
import threading
import uuid
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


MAX_FRAME_SIZE = 1024 * 1024


class EvcsApiError(RuntimeError):
    pass


def receive_exact(connection: socket.socket, size: int) -> bytes:
    chunks: list[bytes] = []
    remaining = size
    while remaining:
        chunk = connection.recv(remaining)
        if not chunk:
            raise EvcsApiError("服务端在响应完成前断开连接")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


class EvcsClient:
    def __init__(self, host: str, port: int) -> None:
        self.connection = socket.create_connection((host, port), timeout=5)
        self.connection.settimeout(8)
        self.token = ""

    def __enter__(self) -> "EvcsClient":
        return self

    def __exit__(self, *_: object) -> None:
        self.connection.close()

    def request(self, action: str, payload: dict[str, Any] | None = None) -> dict[str, Any]:
        request_id = str(uuid.uuid4())
        request: dict[str, Any] = {
            "type": "request",
            "requestId": request_id,
            "action": action,
            "payload": payload or {},
        }
        if self.token:
            request["token"] = self.token
        encoded = json.dumps(request, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(encoded) > MAX_FRAME_SIZE:
            raise EvcsApiError("请求超过协议大小限制")
        self.connection.sendall(struct.pack(">I", len(encoded)) + encoded)
        response_size = struct.unpack(">I", receive_exact(self.connection, 4))[0]
        if response_size > MAX_FRAME_SIZE:
            raise EvcsApiError("服务端响应超过协议大小限制")
        response = json.loads(receive_exact(self.connection, response_size))
        if response.get("requestId") != request_id:
            raise EvcsApiError("服务端响应与请求不匹配")
        if not response.get("ok"):
            error = response.get("error") or {}
            raise EvcsApiError(f"{error.get('code', 'UNKNOWN')}: {error.get('message', '请求失败')}")
        return response.get("data") or {}


class DashboardState:
    def __init__(self, server_host: str, server_port: int, username: str, password: str) -> None:
        self.server_host = server_host
        self.server_port = server_port
        self.username = username
        self.password = password
        self.lock = threading.Lock()

    def call(self, action: str, payload: dict[str, Any] | None = None) -> dict[str, Any]:
        with self.lock, EvcsClient(self.server_host, self.server_port) as client:
            login = client.request("auth.login", {"username": self.username, "password": self.password})
            user = login.get("user") or {}
            if user.get("role") != "admin":
                raise EvcsApiError("配置的账号没有管理员权限")
            client.token = login["token"]
            return client.request(action, payload)


class DashboardHandler(SimpleHTTPRequestHandler):
    server_version = "EVCS-Dashboard/1.0"

    def __init__(self, *args: object, directory: str, state: DashboardState, **kwargs: object) -> None:
        self.state = state
        super().__init__(*args, directory=directory, **kwargs)

    def send_json(self, status: HTTPStatus, body: dict[str, Any]) -> None:
        encoded = json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self) -> None:  # noqa: N802 - inherited API
        if self.path == "/api/dashboard":
            try:
                self.send_json(HTTPStatus.OK, {"ok": True, "data": self.state.call("admin.analytics")})
            except (OSError, ValueError, EvcsApiError) as error:
                self.send_json(HTTPStatus.BAD_GATEWAY, {"ok": False, "message": str(error)})
            return
        if self.path == "/api/health":
            self.send_json(HTTPStatus.OK, {"ok": True, "service": "evcs-dashboard"})
            return
        super().do_GET()

    def do_POST(self) -> None:  # noqa: N802 - inherited API
        if self.path != "/api/generate-demo":
            self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "message": "接口不存在"})
            return
        try:
            result = self.state.call("admin.demo.generateHistory", {"confirmed": True})
            self.send_json(HTTPStatus.OK, {"ok": True, "data": result})
        except (OSError, ValueError, EvcsApiError) as error:
            self.send_json(HTTPStatus.BAD_GATEWAY, {"ok": False, "message": str(error)})

    def log_message(self, message_format: str, *args: object) -> None:
        print(f"dashboard_http peer={self.client_address[0]} message={message_format % args}")


def parse_args() -> argparse.Namespace:
    project_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="充电桩 ECharts 运营大屏本地桥接服务")
    parser.add_argument("--server-host", default="127.0.0.1")
    parser.add_argument("--server-port", type=int, default=45454)
    parser.add_argument("--http-host", default="127.0.0.1")
    parser.add_argument("--http-port", type=int, default=8080)
    parser.add_argument("--username", default=os.environ.get("EVCS_ADMIN_USERNAME", "admin"))
    parser.add_argument("--password", default=os.environ.get("EVCS_ADMIN_PASSWORD", "Admin123!"))
    parser.add_argument("--dashboard-dir", type=Path, default=project_root / "dashboard")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    dashboard_directory = args.dashboard_dir.resolve()
    if not (dashboard_directory / "index.html").is_file():
        raise SystemExit(f"找不到大屏页面：{dashboard_directory / 'index.html'}")
    state = DashboardState(args.server_host, args.server_port, args.username, args.password)

    def handler(*handler_args: object, **handler_kwargs: object) -> DashboardHandler:
        return DashboardHandler(*handler_args, directory=str(dashboard_directory), state=state, **handler_kwargs)

    server = ThreadingHTTPServer((args.http_host, args.http_port), handler)
    print(f"EVCS dashboard: http://{args.http_host}:{args.http_port}")
    print("数据说明：教学演示数据，不代表真实运营结果")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
