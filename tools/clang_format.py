#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
clang-format check/apply helper for this repository.

Usage:
  - Check formatting (CI-friendly):
      python tools/clang_format.py --check
  - Apply formatting in-place:
      python tools/clang_format.py --apply

Notes:
  - Only formats project sources under src/.
  - Skips vendored code by design (we don't scan thirdparty/ at all).
  - Requires clang-format in PATH (any recent version). You can set the
    environment variable CLANG_FORMAT to the full path if needed.
"""
import argparse
import os
import shutil
import subprocess
import sys
from typing import List, Tuple

SUPPORTED_EXTS = {
    ".h", ".hh", ".hpp", ".hxx",
    ".c", ".cc", ".cpp", ".cxx", ".ixx", ".inl",
}


def repo_root() -> str:
    """Locate the repository root (directory that contains src/).

    Strategy:
      1) If inside a git repo and git is available -> `git rev-parse --show-toplevel`.
      2) Walk upwards from this file until a directory containing either
         a .git folder, CMakeLists.txt, or a src/ folder is found.
    """
    # 1) Try git
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            cwd=os.path.dirname(os.path.abspath(__file__)),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        if out.returncode == 0:
            path = out.stdout.strip()
            if path:
                return path
    except Exception:
        pass

    # 2) Walk upwards
    cur = os.path.abspath(os.path.dirname(__file__))
    while True:
        cand_src = os.path.join(cur, "src")
        if os.path.isdir(os.path.join(cur, ".git")) or \
           os.path.isfile(os.path.join(cur, "CMakeLists.txt")) or \
           os.path.isdir(cand_src):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            # Fallback: assume tools/ is under repo root
            return os.path.dirname(os.path.abspath(__file__))
        cur = parent


def find_clang_format() -> str:
    # Allow override via env var
    env = os.environ.get("CLANG_FORMAT") or os.environ.get("CLANG_FORMAT_BIN")
    candidates = [
        env,
        "clang-format",
        "clang-format.exe",
        "clang-format-19",
        "clang-format-18",
        "clang-format-17",
        "clang-format-16",
        "clang-format-15",
    ]
    for c in candidates:
        if not c:
            continue
        path = shutil.which(c) if os.path.sep not in c else (c if os.path.exists(c) else None)
        if path:
            try:
                subprocess.run([path, "--version"], check=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                return path
            except Exception:
                continue
    return ""


def list_source_files(root: str) -> List[str]:
    src_dir = os.path.join(root, "src")
    files: List[str] = []
    for base, _dirs, names in os.walk(src_dir):
        for n in names:
            ext = os.path.splitext(n)[1].lower()
            if ext in SUPPORTED_EXTS:
                files.append(os.path.join(base, n))
    files.sort()
    return files


def run_dry_run(cf_bin: str, file: str) -> Tuple[bool, str]:
    """Return (is_clean, message). Try --dry-run first, fallback to XML parsing.

    Capture bytes to avoid locale decode issues on Windows consoles.
    """
    # Try modern flags
    p = subprocess.run(
        [cf_bin, "--dry-run", "-Werror", "-style=file", file],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )
    if p.returncode in (0, 1):
        # 0: clean; 1: would reformat on most versions
        return (p.returncode == 0, "")

    # Fallback: parse XML replacements (older clang-format)
    p = subprocess.run(
        [cf_bin, "-style=file", "-output-replacements-xml", file],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )
    xml = p.stdout or b""
    needs = b"<replacement " in xml
    return (not needs, "")


def apply_format(cf_bin: str, files: List[str]) -> int:
    changed = 0
    for f in files:
        before = open(f, "rb").read()
        subprocess.run([cf_bin, "-i", "-style=file", f], check=False)
        after = open(f, "rb").read()
        if before != after:
            changed += 1
            print(f"formatted: {os.path.relpath(f, repo_root())}")
    print(f"Total formatted files: {changed}")
    return 0


def check_format(cf_bin: str, files: List[str]) -> int:
    dirty: List[str] = []
    for f in files:
        ok, _msg = run_dry_run(cf_bin, f)
        if not ok:
            dirty.append(f)
    if dirty:
        print("The following files are not clang-formatted:")
        for f in dirty:
            print("  ", os.path.relpath(f, repo_root()))
        print("\nRun: python tools/clang_format.py --apply")
        return 1
    print("All files are properly formatted.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    g = parser.add_mutually_exclusive_group()
    g.add_argument("--check", action="store_true", help="check formatting and exit non-zero if changes are needed")
    g.add_argument("--apply", action="store_true", help="apply clang-format in-place")
    args = parser.parse_args()

    root = repo_root()
    cf_bin = find_clang_format()
    if not cf_bin:
        print("error: clang-format not found in PATH. Install it and/or set CLANG_FORMAT env var.", file=sys.stderr)
        return 2

    files = list_source_files(root)
    if not files:
        print("No source files found under src/; nothing to do.")
        return 0

    if args.apply:
        return apply_format(cf_bin, files)
    # default to check mode if neither specified
    return check_format(cf_bin, files)


if __name__ == "__main__":
    sys.exit(main())
