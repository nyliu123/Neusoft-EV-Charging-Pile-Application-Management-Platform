#!/usr/bin/env python3
"""自动检查教师提出的平铺工程、qmake 与 QSS 结构要求。"""

from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    # 构建产物必须在工程外，因此源码根目录不应出现任何子目录。
    directories = sorted(path.name for path in ROOT.iterdir() if path.is_dir())
    require(not directories, f"项目根目录发现子目录：{directories}")

    application_projects = {
        "evcs_server.pro",
        "evcs_user_client.pro",
        "evcs_admin_client.pro",
    }
    require(all((ROOT / name).is_file() for name in application_projects),
            "三个应用必须分别具有唯一的 .pro 文件")

    # main.cpp 仅装配程序，不承载页面或业务实现。
    for name in ("server_main.cpp", "user_main.cpp", "admin_main.cpp"):
        line_count = len((ROOT / name).read_text(encoding="utf-8").splitlines())
        require(line_count <= 40, f"{name} 不够简洁：{line_count} 行")

    page_styles = {
        "user_login.qss": "userLoginPage",
        "user_station.qss": "userStationPage",
        "user_reservation.qss": "userReservationPage",
        "user_charging.qss": "userChargingPage",
        "user_order.qss": "userOrderPage",
        "user_profile.qss": "userProfilePage",
        "admin_login.qss": "adminLoginPage",
        "admin_dashboard.qss": "adminDashboardPage",
        "admin_station.qss": "adminStationPage",
        "admin_charger.qss": "adminChargerPage",
        "admin_user.qss": "adminUserPage",
        "admin_order.qss": "adminOrderPage",
        "admin_reservation.qss": "adminReservationPage",
        "admin_session.qss": "adminSessionPage",
        "admin_tariff.qss": "adminTariffPage",
        "admin_fault.qss": "adminFaultPage",
    }
    for name, object_name in page_styles.items():
        content = (ROOT / name).read_text(encoding="utf-8")
        require(f"#{object_name}" in content, f"{name} 未绑定页面 {object_name}")

    for qrc_name in ("user_resources.qrc", "admin_resources.qrc"):
        resource = ET.parse(ROOT / qrc_name).getroot().find("qresource")
        require(resource is not None and resource.attrib.get("prefix") == "/qss",
                f"{qrc_name} 必须使用 /qss 前缀")
        names = [node.text or "" for node in resource.findall("file")]
        require(names and all(name.endswith(".qss") for name in names),
                f"{qrc_name} 只能登记 QSS 文件")
        require(all((ROOT / name).is_file() for name in names),
                f"{qrc_name} 存在缺失资源")

    chinese_comment = re.compile(r"//[^\n]*[\u4e00-\u9fff]|#[^\n]*[\u4e00-\u9fff]")
    for name in ("businessservice.cpp", "chargingserver.cpp", "database.cpp",
                 "protocol.cpp", "security.cpp", "preprocess_analytics.py"):
        require(chinese_comment.search((ROOT / name).read_text(encoding="utf-8")) is not None,
                f"{name} 缺少关键代码中文注释")

    print("STRUCTURE_REQUIREMENTS=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
