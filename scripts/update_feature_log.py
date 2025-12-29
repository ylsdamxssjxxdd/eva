#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
用于向 docs/功能迭代.md 写入最新迭代记录的脚本。
默认采用 UTF-8 BOM（utf-8-sig）写入，避免 Windows 环境下出现乱码。
"""

from __future__ import annotations

import argparse
import codecs
from datetime import datetime
from pathlib import Path
import sys


def parse_args() -> argparse.Namespace:
    """解析命令行参数，支持传入迭代内容、日期与目标文件路径。"""
    parser = argparse.ArgumentParser(
        description="将迭代记录写入 docs/功能迭代.md 文件顶部。",
    )
    parser.add_argument(
        "content",
        nargs="+",
        help="迭代内容（不含日期前缀），例如：修复功能迭代记录乱码问题。",
    )
    parser.add_argument(
        "--date",
        default="",
        help="可选，指定日期，格式：YYYY-MM-DD；不传则使用当前日期。",
    )
    parser.add_argument(
        "--path",
        default="docs/功能迭代.md",
        help="功能迭代文档路径（相对仓库根目录或绝对路径）。",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="若顶部已存在相同记录，仍强制插入。",
    )
    parser.add_argument(
        "--no-bom",
        action="store_true",
        help="默认使用 UTF-8 BOM 写入，指定该参数可禁用。",
    )
    return parser.parse_args()


def format_date(date_text: str) -> str:
    """按固定格式输出日期字符串：YYYY年-MM月-DD日。"""
    if date_text:
        try:
            target = datetime.strptime(date_text, "%Y-%m-%d")
        except ValueError as exc:
            raise ValueError("日期格式错误，应为 YYYY-MM-DD") from exc
    else:
        target = datetime.now()
    return f"{target.year}年-{target.month:02d}月-{target.day:02d}日"


def resolve_target_path(path_text: str) -> Path:
    """将传入路径解析为绝对路径，未传绝对路径则基于仓库根目录解析。"""
    path = Path(path_text)
    if path.is_absolute():
        return path
    repo_root = Path(__file__).resolve().parents[1]
    return (repo_root / path).resolve()


def read_existing_content(path: Path) -> tuple[str, str]:
    """读取已有内容，返回：文本内容与换行符类型。"""
    if not path.exists():
        return "", "\r\n"

    data = path.read_bytes()
    text = data.decode("utf-8-sig")
    newline = "\r\n" if "\r\n" in text else "\n"
    return text, newline


def build_new_content(
    entry_line: str,
    original_text: str,
    newline: str,
) -> str:
    """将新记录插入到文档顶部，并保留原有内容。"""
    if not original_text:
        return entry_line + newline
    return entry_line + newline + original_text


def main() -> int:
    """主流程：读取、插入、编码写回，确保中文不乱码。"""
    args = parse_args()
    content = " ".join(args.content).strip()
    if not content:
        print("迭代内容不能为空。", file=sys.stderr)
        return 1

    try:
        date_prefix = format_date(args.date)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    entry_line = f"{date_prefix}：{content}"
    target_path = resolve_target_path(args.path)
    # 确保目标目录存在，避免首次写入时报错。
    target_path.parent.mkdir(parents=True, exist_ok=True)

    # 读取原始内容，用来保持换行风格并避免重复插入。
    original_text, newline = read_existing_content(target_path)
    first_line = original_text.splitlines()[0] if original_text else ""
    if first_line == entry_line and not args.force:
        print("顶部已存在相同记录，未重复写入。")
        return 0

    new_text = build_new_content(entry_line, original_text, newline)

    # 统一使用 UTF-8 编码写回；默认保留 BOM，避免 Windows 记事本显示乱码。
    use_bom = not args.no_bom
    encoded = (codecs.BOM_UTF8 if use_bom else b"") + new_text.encode("utf-8")

    target_path.write_bytes(encoded)
    print(f"已写入：{target_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
