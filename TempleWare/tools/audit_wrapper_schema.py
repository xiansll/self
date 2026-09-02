#!/usr/bin/env python3
"""Read-only audit for the P3D wrapper diagnostics.

Compares the offsets that TempleWare's static SchemaFinder table will return
against the repository's current alldump/client_dll.json. This tool never
rewrites source files or generated offsets.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DUMP_PATH = ROOT / "alldump" / "client_dll.json"
TABLE_PATH = (
    ROOT
    / "TempleWare-CS2"
    / "source"
    / "templeware"
    / "utils"
    / "schema"
    / "schema_offsets_table.h"
)

# Exact read-only fields used by the P3D semantic and pointer-identity probes.
WRAPPER_DIAGNOSTIC_FIELDS = (
    "C_BaseEntity->m_iMaxHealth",
    "C_BaseEntity->m_iHealth",
    "C_BaseEntity->m_iTeamNum",
    "CBasePlayerController->m_hPawn",
    "CBasePlayerController->m_bIsLocalPlayerController",
    "CCSPlayerController->m_bPawnIsAlive",
)

ENTRY_RE = re.compile(
    r"\{\s*(0x[0-9A-Fa-f]+)u?\s*,\s*(0x[0-9A-Fa-f]+)u?\s*\}\s*,?\s*//\s*([^\r\n]+)"
)


def fnv1a32(text: str) -> int:
    value = 0x811C9DC5
    for byte in text.encode("utf-8"):
        value = ((value ^ byte) * 0x01000193) & 0xFFFFFFFF
    return value


def load_client_classes() -> dict[str, dict]:
    with DUMP_PATH.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)

    module = payload.get("client.dll")
    if not isinstance(module, dict):
        raise RuntimeError("client_dll.json has no 'client.dll' object")

    classes = module.get("classes")
    if not isinstance(classes, dict):
        raise RuntimeError("client_dll.json has no client.dll.classes object")

    return classes


def resolve_dump_field(classes: dict[str, dict], field_path: str) -> int | None:
    class_name, field_name = field_path.split("->", 1)
    seen: set[str] = set()

    while class_name and class_name not in seen:
        seen.add(class_name)
        info = classes.get(class_name)
        if not isinstance(info, dict):
            return None

        fields = info.get("fields", {})
        if isinstance(fields, dict) and field_name in fields:
            value = fields[field_name]
            return int(value) if isinstance(value, int) else None

        parent = info.get("parent")
        class_name = parent if isinstance(parent, str) and parent else ""

    return None


def load_table() -> dict[int, tuple[int, str]]:
    text = TABLE_PATH.read_text(encoding="utf-8")
    rows: dict[int, tuple[int, str]] = {}

    for match in ENTRY_RE.finditer(text):
        hash_value = int(match.group(1), 16)
        offset = int(match.group(2), 16)
        label = match.group(3).strip()
        rows[hash_value] = (offset, label)

    if not rows:
        raise RuntimeError("no schema entries parsed from schema_offsets_table.h")

    return rows


def main() -> int:
    for path in (DUMP_PATH, TABLE_PATH):
        if not path.is_file():
            print(f"ERROR: missing {path}")
            return 2

    try:
        classes = load_client_classes()
        table = load_table()
    except (OSError, json.JSONDecodeError, RuntimeError, ValueError) as exc:
        print(f"ERROR: {exc}")
        return 2

    failures = 0
    print("P3D wrapper schema audit")
    print(f"dump : {DUMP_PATH}")
    print(f"table: {TABLE_PATH}")
    print()

    for field_path in WRAPPER_DIAGNOSTIC_FIELDS:
        expected_hash = fnv1a32(field_path)
        dump_offset = resolve_dump_field(classes, field_path)
        table_entry = table.get(expected_hash)

        if dump_offset is None:
            failures += 1
            print(
                f"DUMP_MISSING {field_path} hash=0x{expected_hash:08X} "
                "dump=<missing>"
            )
            continue

        if table_entry is None:
            failures += 1
            print(
                f"TABLE_MISSING {field_path} hash=0x{expected_hash:08X} "
                f"dump=0x{dump_offset:X}"
            )
            continue

        table_offset, table_label = table_entry
        if table_offset != dump_offset:
            failures += 1
            print(
                f"MISMATCH {field_path} hash=0x{expected_hash:08X} "
                f"table=0x{table_offset:X} dump=0x{dump_offset:X} "
                f"table_label={table_label}"
            )
            continue

        print(
            f"OK       {field_path} hash=0x{expected_hash:08X} "
            f"offset=0x{dump_offset:X}"
        )

    print()
    if failures:
        print(
            f"RESULT: BLOCKED ({failures} mismatch/missing entr"
            f"{'y' if failures == 1 else 'ies'}). "
            "Do not open the runtime wrapper trust gate."
        )
        return 1

    print(
        "RESULT: STATIC PARITY PASS. The TempleWare table matches the checked "
        "alldump fields. Runtime P3D must still prove pointer/layout semantics."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
