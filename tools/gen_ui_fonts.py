#!/usr/bin/env python3
import pathlib
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "components" / "UI" / "fonts" / "generated"

FONT_REGULAR = pathlib.Path("C:/Windows/Fonts/Deng.ttf")
FONT_BOLD = pathlib.Path("C:/Windows/Fonts/Dengb.ttf")
FONT_BY_SIZE = {
    11: FONT_REGULAR,
    12: FONT_REGULAR,
    13: FONT_REGULAR,
    14: FONT_REGULAR,
    18: FONT_BOLD,
    24: FONT_BOLD,
    30: FONT_BOLD,
}

TEXT_SAMPLES = [
    "14:28 26°C 26℃ 体感28°C 体感28℃",
    "上海 · 多云",
    "当前天气 多云转晴 湿度 东南风 级 AQI 良",
    "进入设置 设备状态",
    "支持天气、定位、设备信息联动展示",
    "← 返回 设置 可滚动",
    "WiFi连接 点击进入扫描 使用地点 上海 · 浦东新区 在线 / 正常",
    "天气刷新频率 每15分钟 语言 简体中文 时区 Asia/Shanghai 保存并返回 WiFi",
    "供电状态 WiFi信号 IP地址 固件版本 运行时长 内存占用 上次同步 刷新状态 返回首页",
    "USB 5V / 0.42A -54 dBm 192.168.1.43 v1.2.7 03:27:51 63% 14:20:33",
    "← 设置 WiFi连接 扫描",
    "扫描结果 连接信息 连接WiFi 取消",
    "连接失败：密码错误，请重新输入",
    "连接失败：请输入密码 连接成功：WiFi 已连接",
    "强 中 弱",
    "Office_2.4G Office_5G Guest_WiFi",
    "、·：%",
]


def build_symbols() -> str:
    symbols = set()
    for text in TEXT_SAMPLES:
        symbols.update(text)
    symbols = {ch for ch in symbols if ord(ch) > 0x7E}
    return "".join(sorted(symbols))


def gen_font(size: int, symbols: str) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_c = OUT_DIR / f"ui_font_sc_{size}.c"
    font_path = FONT_BY_SIZE[size]

    npx_cmd = shutil.which("npx.cmd") or shutil.which("npx")
    if npx_cmd is None:
        raise RuntimeError("npx not found")

    cmd = [
        npx_cmd,
        "--yes",
        "lv_font_conv",
        "--size",
        str(size),
        "--bpp",
        "4",
        "--no-compress",
        "--format",
        "lvgl",
        "--font",
        str(font_path),
        "-r",
        "0x20-0x7E",
        "--symbols",
        symbols,
        "--lv-font-name",
        f"ui_font_sc_{size}",
        "--force-fast-kern-format",
        "-o",
        str(out_c),
    ]
    subprocess.run(cmd, check=True)
    print(f"generated: {out_c}")


def main() -> int:
    required = sorted(set(FONT_BY_SIZE.values()))
    for f in required:
        if not f.exists():
            print(f"font not found: {f}", file=sys.stderr)
            return 1

    symbols = build_symbols()
    for size in sorted(FONT_BY_SIZE.keys()):
        gen_font(size, symbols)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
