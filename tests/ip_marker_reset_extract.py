#!/usr/bin/env python3
"""IP progression-marker reset regression harness: read-only extraction + contracts.

TEST-ONLY. Nothing here is compiled into, linked with or loaded by Worldserver.

This module reads three pinned, read-only source trees:

* mod-playerbots  e88cc4099e69f35126b28942a412ca702d20453b
* AzerothCore     Grimfeather/azerothcore-wotlk 8daffdabe9202c228e45d833c9a1d399597a15cd
* this IP fork    (the checkout under test)

It extracts exact C++ function bodies, enums and SQL rows from them and checks
the chain by which Playerbots' full randomisation removes IP's hidden
progression-stage quest markers 66001..66018:

    RandomPlayerbotMgr::Randomize / handlers["init"] / handlers["clear"]
      -> PlayerbotFactory::Randomize(false) / ClearEverything()
      -> PlayerbotFactory::ResetQuests()
      -> Player::GetQuestStatus(entry) != QUEST_STATUS_NONE
      -> Player::RemoveRewardedQuest(entry)  (m_RewardedQuestsSave[entry] = false)
      -> Player::_SaveQuestStatus -> CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST

Each ``check_*`` function raises ContractViolation when the pinned source no
longer matches the recorded chain. A green result is SOURCE evidence for the
historical pins. It does NOT show what the owner's installed server runs and
it is not a Worldserver, database or gameplay test.

It can also generate a translation unit for the compiled extracted-body
harness (``tests/marker_reset_harness``): ``python3 tests/ip_marker_reset_extract.py
generate --out DIR``.
"""
from __future__ import annotations

import argparse
import functools
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Callable, Dict, Iterable, Iterator, List, Optional, Sequence, Tuple

PINNED_PLAYERBOTS_SHA = "e88cc4099e69f35126b28942a412ca702d20453b"
PINNED_CORE_SHA = "8daffdabe9202c228e45d833c9a1d399597a15cd"
# Review-only comparison pin (CS02-IP's earlier PB revision; ancestor of e88cc409 by 53 commits).
# Used ONLY to compare config key names/defaults. Never a substitute for the primary pin.
LEGACY_PLAYERBOTS_SHA = "037c01418b5d01506917a3db9b44fd56ac5f965c"
LEGACY_PB_FILES = {"config": "src/PlayerbotAIConfig.cpp", "conf_dist": "conf/playerbots.conf.dist"}

STAGE_MARKER_BASE = 66000
EXPECTED_STAGE_MARKERS = tuple(range(66001, 66019))  # 66001..66018 inclusive
PVP_MARKER_RANGE = range(66101, 66115)  # PvP rank markers: NOT stage markers

# Repository-relative paths of every pinned file the harness reads.
PB_FILES = {
    "factory": "src/Bot/Factory/PlayerbotFactory.cpp",
    "mgr": "src/Bot/RandomPlayerbotMgr.cpp",
    "config": "src/PlayerbotAIConfig.cpp",
    "conf_dist": "conf/playerbots.conf.dist",
    "playerbot_mgr": "src/Bot/PlayerbotMgr.cpp",
    "level_mgr": "src/Bot/RandomBotLevelMgr.cpp",
}
CORE_FILES = {
    "player_quest": "src/server/game/Entities/Player/PlayerQuest.cpp",
    "player_storage": "src/server/game/Entities/Player/PlayerStorage.cpp",
    "player_h": "src/server/game/Entities/Player/Player.h",
    "quest_def_h": "src/server/game/Quests/QuestDef.h",
    "quest_def_cpp": "src/server/game/Quests/QuestDef.cpp",
    "object_mgr": "src/server/game/Globals/ObjectMgr.cpp",
    "object_mgr_h": "src/server/game/Globals/ObjectMgr.h",
    "shared_defines": "src/server/shared/SharedDefines.h",
}
IP_FILES = {
    "ip_cpp": "src/IndividualProgression.cpp",
    "ip_h": "src/IndividualProgression.h",
    "ip_player": "src/IndividualProgressionPlayer.cpp",
    "progression_sql": "data/sql/world/base/player_progression.sql",
}
IP_SQL_DIRS = ("data/sql", "optional/sql")


class ContractViolation(AssertionError):
    """The pinned source no longer matches the recorded marker-reset chain."""


class ExtractionError(ContractViolation):
    """A construct the harness needs could not be found unambiguously."""


# --------------------------------------------------------------------------
# C++ text helpers
# --------------------------------------------------------------------------

def _skip_literal(text: str, i: int) -> int:
    """Return index just past the string/char literal that starts at text[i]."""
    quote = text[i]
    j = i + 1
    n = len(text)
    while j < n and text[j] != quote:
        if text[j] == "\\":
            j += 1
        elif text[j] == "\n" and quote == "'":
            # Not a char literal (e.g. a stray apostrophe); treat as one char.
            return i + 1
        j += 1
    return j + 1


@functools.lru_cache(maxsize=256)
def mask_comments(src: str) -> str:
    """Blank out // and /* */ comments, keeping offsets and newlines intact.

    String and character literals are preserved, so a ``//`` inside a string is
    not mistaken for a comment. Commented-out code (e.g. ``// handlers["initmin"]``)
    therefore never satisfies a contract.
    """
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j == -1 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j == -1 else j + 2
            for k in range(i, j):
                if src[k] != "\n":
                    out[k] = " "
            i = j
        elif c in "\"'":
            i = _skip_literal(src, i)
        else:
            i += 1
    return "".join(out)


def match_bracket(masked: str, open_idx: int) -> int:
    """Index of the bracket closing masked[open_idx] (one of ( [ {)."""
    pairs = {"(": ")", "[": "]", "{": "}"}
    open_ch = masked[open_idx]
    close_ch = pairs[open_ch]
    depth = 0
    i = open_idx
    n = len(masked)
    while i < n:
        c = masked[i]
        if c in "\"'":
            i = _skip_literal(masked, i)
            continue
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ExtractionError(f"unbalanced {open_ch!r} at offset {open_idx}")


def normalize_ws(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


@dataclass(frozen=True)
class CppFunction:
    name: str
    label: str
    line: int
    text: str          # verbatim original text, signature through closing brace
    body: str          # comment-masked text strictly inside the outer braces
    body_original: str  # verbatim text strictly inside the outer braces

    @property
    def flat(self) -> str:
        """Comment-free, whitespace-normalised body for contract matching."""
        return normalize_ws(self.body)


def find_function(src: str, name: str, label: str = "") -> CppFunction:
    """Find exactly one *definition* (not a call/declaration) of ``name``.

    ``name`` may be qualified (``Player::GetQuestStatus``) or an inline member
    name (``IsQuestRewarded``). A definition is the name, a balanced
    parameter list, optional ``const``/``noexcept``/``override``, then ``{``.
    """
    masked = mask_comments(src)
    pattern = re.compile(r"(?m)^[^\n;{}]*?(?<![\w:])" + re.escape(name) + r"\s*\(")
    found: List[CppFunction] = []
    for m in pattern.finditer(masked):
        paren_open = m.end() - 1
        paren_close = match_bracket(masked, paren_open)
        tail = re.match(r"\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?\{", masked[paren_close + 1:])
        if not tail:
            continue
        body_open = paren_close + tail.end()
        body_close = match_bracket(masked, body_open)
        start = m.start()
        directive = re.search(r"(?m)^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b", masked[body_open + 1:body_close])
        if directive:
            raise ExtractionError(
                f"{name} in {label or 'source'}: conditional preprocessor '#{directive.group(1)}' inside the "
                "extracted body; a text contract cannot tell which branch is compiled (fail closed)")
        found.append(CppFunction(
            name=name,
            label=label,
            line=src.count("\n", 0, start) + 1,
            text=src[start:body_close + 1],
            body=masked[body_open + 1:body_close],
            body_original=src[body_open + 1:body_close],
        ))
    if len(found) != 1:
        raise ExtractionError(
            f"expected exactly one definition of {name} in {label or 'source'}, found {len(found)}")
    return found[0]


def find_enum(src: str, name: str, label: str = "") -> str:
    """Verbatim ``enum name ... { ... };`` text (exactly one occurrence)."""
    masked = mask_comments(src)
    hits = list(re.finditer(r"(?m)^enum\s+" + re.escape(name) + r"\b[^{;]*\{", masked))
    if len(hits) != 1:
        raise ExtractionError(f"expected one enum {name} in {label or 'source'}, found {len(hits)}")
    m = hits[0]
    close = match_bracket(masked, m.end() - 1)
    semi = masked.index(";", close)
    return src[m.start():semi + 1]


def enum_values(enum_text: str) -> Dict[str, int]:
    """Evaluate simple ``NAME = <int literal>`` enumerators (others ignored)."""
    values: Dict[str, int] = {}
    for m in re.finditer(r"\b([A-Z][A-Z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\b\s*[,}\n]",
                         mask_comments(enum_text)):
        values[m.group(1)] = int(m.group(2), 0)
    return values


def find_define(src: str, name: str) -> str:
    hits = re.findall(r"(?m)^#define\s+" + re.escape(name) + r"\s+.*$", src)
    if len(hits) != 1:
        raise ExtractionError(f"expected one #define {name}, found {len(hits)}")
    return hits[0].strip()


def find_statements(fn: CppFunction, head_regex: str) -> List[Tuple[str, str]]:
    """Every ``<head>(...) { ... }`` inside fn as (verbatim, masked) pairs."""
    masked = fn.body
    out: List[Tuple[str, str]] = []
    for m in re.finditer(head_regex, masked):
        paren_open = masked.index("(", m.start())
        paren_close = match_bracket(masked, paren_open)
        brace = re.match(r"\s*\{", masked[paren_close + 1:])
        if not brace:
            raise ExtractionError(f"{fn.name}: statement /{head_regex}/ has no braced block")
        open_idx = paren_close + brace.end()
        close_idx = match_bracket(masked, open_idx)
        out.append((fn.body_original[m.start():close_idx + 1], masked[m.start():close_idx + 1]))
    return out


def find_statement(fn: CppFunction, head_regex: str) -> Tuple[str, str]:
    """Exactly one ``<head>(...) { ... }`` inside fn; return (verbatim, masked)."""
    hits = find_statements(fn, head_regex)
    if len(hits) != 1:
        raise ExtractionError(f"{fn.name}: expected one statement /{head_regex}/, found {len(hits)}")
    return hits[0]


def braced_inner(masked_stmt: str) -> str:
    """Text strictly inside the first top-level ``{ ... }`` of a statement."""
    open_idx = masked_stmt.index("{")
    return masked_stmt[open_idx + 1:match_bracket(masked_stmt, open_idx)]


def top_level_statements(block: str) -> List[str]:
    """Split comment-masked block contents into normalised top-level statements.

    ``if (c) x;`` and ``if (c) { x; } else { y; }`` are ONE statement each, so a
    call wrapped in any guard is never reported as a top-level statement.
    """
    out: List[str] = []
    buf: List[str] = []
    paren = brace = 0
    i, n = 0, len(block)
    while i < n:
        c = block[i]
        if c in "\"'":
            j = _skip_literal(block, i)
            buf.append(block[i:j])
            i = j
            continue
        buf.append(c)
        if c == "(":
            paren += 1
        elif c == ")":
            paren -= 1
        elif c == "{":
            brace += 1
        elif c == "}":
            brace -= 1
            if brace == 0 and paren == 0 and not re.match(r"\s*else\b", block[i + 1:]):
                out.append(normalize_ws("".join(buf)))
                buf = []
        elif c == ";" and paren == 0 and brace == 0:
            out.append(normalize_ws("".join(buf)))
            buf = []
        i += 1
    tail = normalize_ws("".join(buf))
    if tail:
        out.append(tail)
    return [s for s in out if s]


def concatenated_string_literal(code: str) -> str:
    """Join adjacent C string literals in a masked code fragment."""
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', code))


# --------------------------------------------------------------------------
# SQL helpers (MySQL dump style used by IP)
# --------------------------------------------------------------------------

def iter_sql_statements(text: str) -> Iterator[str]:
    """Split on ';' outside quotes and comments; comments are dropped."""
    buf: List[str] = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "'\"`":
            j = i + 1
            while j < n:
                if text[j] == "\\" and c != "`":
                    j += 2
                    continue
                if text[j] == c:
                    if j + 1 < n and text[j + 1] == c:  # doubled quote escape
                        j += 2
                        continue
                    break
                j += 1
            buf.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("--", i) or c == "#":
            j = text.find("\n", i)
            i = n if j == -1 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j == -1 else j + 2
        elif c == ";":
            stmt = "".join(buf).strip()
            if stmt:
                yield stmt
            buf = []
            i += 1
        else:
            buf.append(c)
            i += 1
    tail = "".join(buf).strip()
    if tail:
        yield tail


def _split_sql_values(row: str) -> List[object]:
    """Split one tuple body into typed values (int, float, str, None, raw)."""
    out: List[object] = []
    cur: List[str] = []
    quoted: Optional[str] = None
    i, n = 0, len(row)
    depth = 0

    def flush():
        raw = "".join(cur).strip()
        if quoted is not None:
            out.append(quoted)
        elif raw.upper() == "NULL":
            out.append(None)
        elif re.fullmatch(r"[-+]?\d+", raw):
            out.append(int(raw))
        elif re.fullmatch(r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?", raw):
            out.append(float(raw))
        else:
            out.append(raw)

    while i < n:
        c = row[i]
        if c in "'\"" and quoted is None and not "".join(cur).strip():
            j = i + 1
            chars: List[str] = []
            while j < n:
                if row[j] == "\\":
                    chars.append(row[j + 1] if j + 1 < n else "")
                    j += 2
                    continue
                if row[j] == c:
                    if j + 1 < n and row[j + 1] == c:
                        chars.append(c)
                        j += 2
                        continue
                    break
                chars.append(row[j])
                j += 1
            quoted = "".join(chars)
            i = j + 1
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        if c == "," and depth == 0:
            flush()
            cur, quoted = [], None
        else:
            cur.append(c)
        i += 1
    flush()
    return out


@dataclass(frozen=True)
class SqlInsert:
    table: str
    columns: Tuple[str, ...]
    rows: Tuple[Dict[str, object], ...]


_INSERT_RE = re.compile(
    r"^\s*(?:INSERT|REPLACE)\s+(?:IGNORE\s+)?INTO\s+`?(\w+)`?\s*\(([^)]*)\)\s*VALUES\s*(.*)$",
    re.IGNORECASE | re.DOTALL)


def _value_tuples(values: str) -> List[List[object]]:
    """Every top-level ``( ... )`` tuple of a VALUES list, split into typed values."""
    rows: List[List[object]] = []
    i, n = 0, len(values)
    while i < n:
        if values[i] == "(":
            j = i + 1
            depth = 1
            while j < n and depth:
                ch = values[j]
                if ch in "'\"":
                    j += 1
                    while j < n:
                        if values[j] == "\\":
                            j += 2
                            continue
                        if values[j] == ch:
                            if j + 1 < n and values[j + 1] == ch:
                                j += 2
                                continue
                            break
                        j += 1
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                j += 1
            rows.append(_split_sql_values(values[i + 1:j - 1]))
            i = j
        else:
            i += 1
    return rows


def parse_insert(stmt: str) -> Optional[SqlInsert]:
    m = _INSERT_RE.match(stmt)
    if not m:
        return None
    table = m.group(1)
    columns = tuple(c.strip().strip("`") for c in m.group(2).split(","))
    rows: List[Dict[str, object]] = []
    for vals in _value_tuples(m.group(3)):
        if len(vals) != len(columns):
            raise ExtractionError(f"{table}: row has {len(vals)} values for {len(columns)} columns: {vals[:4]!r}")
        rows.append(dict(zip(columns, vals)))
    return SqlInsert(table=table, columns=columns, rows=tuple(rows))


_POSITIONAL_INSERT_RE = re.compile(
    r"^\s*(?:INSERT|REPLACE)\s+(?:IGNORE\s+)?INTO\s+`?(\w+)`?\s*VALUES\s*(.*)$", re.IGNORECASE | re.DOTALL)


def _ids_in_where(where: str) -> Iterable[int]:
    for a, b in re.findall(r"BETWEEN\s+(\d+)\s+AND\s+(\d+)", where, re.IGNORECASE):
        yield from range(int(a), int(b) + 1)
    for num in re.findall(r"\b\d+\b", where):
        yield int(num)


def statements_touching_ids(sql_text: str, tables: Sequence[str], ids: Iterable[int]) -> List[str]:
    """Statements on ``tables`` that insert/update/delete any of ``ids``."""
    return list(_statements_touching_ids_cached(sql_text, tuple(tables), frozenset(ids)))


@functools.lru_cache(maxsize=1024)
def _statements_touching_ids_cached(sql_text: str, tables: Tuple[str, ...], wanted: frozenset) -> Tuple[str, ...]:
    hits: List[str] = []
    table_alt = "|".join(re.escape(t) for t in tables)
    # Cheap prefilter: files that never name the tables cannot touch them.
    if not re.search(r"\b(?:" + table_alt + r")\b", sql_text, re.IGNORECASE):
        return ()
    for stmt in iter_sql_statements(sql_text):
        if not re.match(r"\s*(?:INSERT|REPLACE|UPDATE|DELETE)\b[^(]*?\b(?:" + table_alt + r")\b", stmt, re.IGNORECASE):
            continue
        ins = parse_insert(stmt)
        if ins is not None:
            if ins.table in tables and any(r.get("ID") in wanted for r in ins.rows):
                hits.append(f"{ins.table}: INSERT/REPLACE")
            continue
        if re.match(r"\s*(?:INSERT|REPLACE)\b", stmt, re.IGNORECASE):
            # Fail closed: an insert on a marker table we cannot read is a finding, never a skip.
            pos = _POSITIONAL_INSERT_RE.match(stmt)
            if pos and pos.group(1) in tables:
                # Column-less VALUES: ID is column 0 of both quest_template and quest_template_addon.
                if any(row and row[0] in wanted for row in _value_tuples(pos.group(2))):
                    hits.append(f"{pos.group(1)}: column-less INSERT/REPLACE")
                continue
            hits.append(f"UNPARSEABLE insert on a marker table (fail closed): {normalize_ws(stmt)[:80]}")
            continue
        m = re.match(r"^\s*(UPDATE|DELETE\s+FROM)\s+`?(" + table_alt + r")`?\b(.*)$",
                     stmt, re.IGNORECASE | re.DOTALL)
        if not m:
            continue
        where = re.split(r"\bWHERE\b", m.group(3), maxsplit=1, flags=re.IGNORECASE)
        if len(where) == 2 and wanted.intersection(_ids_in_where(where[1])):
            hits.append(f"{m.group(2)}: {m.group(1).split()[0].upper()} {normalize_ws(where[1])[:80]}")
    return tuple(hits)


# --------------------------------------------------------------------------
# Source bundle
# --------------------------------------------------------------------------

@dataclass(frozen=True)
class Sources:
    """In-memory copies of every pinned file. Mutation fixtures edit copies."""
    pb: Dict[str, str]
    core: Dict[str, str]
    ip: Dict[str, str]
    ip_sql_tree: Dict[str, str]  # every *.sql under data/sql and optional/sql
    roots: Dict[str, Path] = field(default_factory=dict)
    pb_src_tree: Dict[str, str] = field(default_factory=dict)  # every *.cpp/*.h under PB src/
    ip_src_tree: Dict[str, str] = field(default_factory=dict)  # every *.cpp/*.h under IP src/

    def with_edit(self, group: str, key: str, transform: Callable[[str], str]) -> "Sources":
        """Return a copy with one file transformed (used by negative fixtures)."""
        files = dict(getattr(self, group))
        before = files[key]
        after = transform(before)
        if after == before:
            raise ExtractionError(f"mutation of {group}:{key} changed nothing (anchor missing?)")
        files[key] = after
        return replace(self, **{group: files})


def default_roots() -> Dict[str, Path]:
    here = Path(__file__).resolve().parent.parent
    return {
        "pb": Path(os.environ.get("PINNED_PLAYERBOTS_ROOT", "historical-playerbots")),
        "core": Path(os.environ.get("PINNED_CORE_ROOT", "historical-core")),
        "ip": Path(os.environ.get("PINNED_IP_ROOT", str(here))),
    }


def load_sources(roots: Optional[Dict[str, Path]] = None) -> Sources:
    roots = roots or default_roots()

    def read(root: Path, rel: str) -> str:
        p = root / rel
        if not p.is_file():
            raise ExtractionError(f"missing pinned file {p}")
        return p.read_text(encoding="utf-8", errors="replace")

    sql_tree: Dict[str, str] = {}
    for d in IP_SQL_DIRS:
        base = roots["ip"] / d
        if base.is_dir():
            for p in sorted(base.rglob("*.sql")):
                sql_tree[p.relative_to(roots["ip"]).as_posix()] = p.read_text(encoding="utf-8", errors="replace")
    def tree(root: Path, sub: str, patterns: Tuple[str, ...]) -> Dict[str, str]:
        out: Dict[str, str] = {}
        base = root / sub
        if base.is_dir():
            for pat in patterns:
                for p in sorted(base.rglob(pat)):
                    out[p.relative_to(root).as_posix()] = p.read_text(encoding="utf-8", errors="replace")
        return out

    return Sources(
        pb_src_tree=tree(roots["pb"], "src", ("*.cpp", "*.h")),
        ip_src_tree=tree(roots["ip"], "src", ("*.cpp", "*.h")),
        pb={k: read(roots["pb"], v) for k, v in PB_FILES.items()},
        core={k: read(roots["core"], v) for k, v in CORE_FILES.items()},
        ip={k: read(roots["ip"], v) for k, v in IP_FILES.items()},
        ip_sql_tree=sql_tree,
        roots=dict(roots),
    )


def git_is_clean(root: Path) -> bool:
    """True when ``git status --porcelain`` (incl. untracked) is empty for the checkout."""
    try:
        out = subprocess.run(["git", "-C", str(root), "status", "--porcelain", "--untracked-files=all"],
                             capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return False
    return out.stdout.strip() == ""


def git_head(root: Path) -> Optional[str]:
    try:
        out = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                             capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return None
    return out.stdout.strip()


# --------------------------------------------------------------------------
# Facts derived from the sources
# --------------------------------------------------------------------------

@dataclass(frozen=True)
class MarkerTemplate:
    quest_id: int
    quest_sort_id: int
    flags: int
    special_flags: int
    effective_special_flags: int  # after the pinned Core loader normalisation

    def is_repeatable(self, repeatable_bit: int) -> bool:
        return bool(self.effective_special_flags & repeatable_bit)

    def is_seasonal(self, c: "CoreQuestConstants") -> bool:
        """Mirror of pinned Quest::IsSeasonal (ZoneOrSort == -QUEST_SORT_x && !repeatable)."""
        return (-self.quest_sort_id) in c.seasonal_sorts and not self.is_repeatable(c.repeatable)


@dataclass(frozen=True)
class CoreQuestConstants:
    repeatable: int
    monthly: int
    db_allowed_mask_names: Tuple[str, ...]
    special_values: Dict[str, int]
    flag_daily: int
    flag_weekly: int
    seasonal_sorts: Tuple[int, ...]  # positive QuestSort values IsSeasonal() negates


def core_quest_constants(src: Sources) -> CoreQuestConstants:
    qh = src.core["quest_def_h"]
    special_enum = find_enum(qh, "QuestSpecialFlags", "QuestDef.h")
    special = enum_values(special_enum)
    allowed_m = re.search(r"QUEST_SPECIAL_FLAGS_DB_ALLOWED\s*=\s*([^,}]*?),\s*\n\s*\n",
                          mask_comments(special_enum), re.DOTALL)
    allowed_names = tuple(re.findall(r"QUEST_SPECIAL_FLAGS_\w+", allowed_m.group(1))) if allowed_m else ()
    flags = enum_values(find_enum(qh, "QuestFlags", "QuestDef.h"))
    sorts = enum_values(find_enum(src.core["shared_defines"], "QuestSort", "SharedDefines.h"))
    is_seasonal = find_function(qh, "IsSeasonal", "QuestDef.h")
    seasonal_names = re.findall(r"ZoneOrSort\s*==\s*-\s*(QUEST_SORT_\w+)", is_seasonal.body)
    return CoreQuestConstants(
        repeatable=special["QUEST_SPECIAL_FLAGS_REPEATABLE"],
        monthly=special["QUEST_SPECIAL_FLAGS_MONTHLY"],
        db_allowed_mask_names=allowed_names,
        special_values=special,
        flag_daily=flags["QUEST_FLAGS_DAILY"],
        flag_weekly=flags["QUEST_FLAGS_WEEKLY"],
        seasonal_sorts=tuple(sorts[n] for n in seasonal_names),
    )


def core_effective_special_flags(raw_special: int, flags: int, c: CoreQuestConstants) -> int:
    """Pinned ObjectMgr::LoadQuests normalisation (see check_core_loader_normalisation)."""
    allowed = 0
    for name in c.db_allowed_mask_names:
        allowed |= c.special_values[name]
    sf = raw_special & allowed
    if flags & c.flag_daily and flags & c.flag_weekly:
        flags &= ~c.flag_daily
    if flags & c.flag_daily:
        sf |= c.repeatable
    if flags & c.flag_weekly:
        sf |= c.repeatable
    if sf & c.monthly:
        sf |= c.repeatable
    return sf


def ip_marker_templates(src: Sources, ids: Iterable[int] = EXPECTED_STAGE_MARKERS) -> Dict[int, MarkerTemplate]:
    wanted = set(ids)
    tmpl: Dict[int, Dict[str, object]] = {}
    addon: Dict[int, Dict[str, object]] = {}
    for stmt in iter_sql_statements(src.ip["progression_sql"]):
        ins = parse_insert(stmt)
        if ins is None:
            continue
        for row in ins.rows:
            qid = row.get("ID")
            if qid not in wanted:
                continue
            target = tmpl if ins.table == "quest_template" else addon if ins.table == "quest_template_addon" else None
            if target is None:
                continue
            if qid in target:
                raise ContractViolation(f"{ins.table} defines {qid} twice in player_progression.sql")
            target[qid] = row
    consts = core_quest_constants(src)
    out: Dict[int, MarkerTemplate] = {}
    for qid in sorted(tmpl):
        t = tmpl[qid]
        a = addon.get(qid, {})
        raw_sf = int(a.get("SpecialFlags", 0) or 0)
        flags = int(t["Flags"])
        out[qid] = MarkerTemplate(
            quest_id=qid,
            quest_sort_id=int(t["QuestSortID"]),
            flags=flags,
            special_flags=raw_sf,
            effective_special_flags=core_effective_special_flags(raw_sf, flags, consts),
        )
    missing_addon = sorted(set(tmpl) - set(addon))
    if missing_addon:
        raise ContractViolation(f"quest_template_addon rows missing for {missing_addon}")
    return out


def ip_progression_quest_ids(src: Sources) -> Tuple[int, ...]:
    """Marker IDs IP reads, derived from GetPlayerProgressionFromQuests + enum."""
    fn = find_function(src.ip["ip_cpp"], "IndividualProgression::GetPlayerProgressionFromQuests", "IndividualProgression.cpp")
    flat = fn.flat
    loop = re.search(r"for \( ?uint8 i = (PROGRESSION_\w+); i <= (PROGRESSION_\w+); \+\+i\)", flat)
    base = re.search(r"uint32 PROGRESSION_QUEST = (\d+) \+ i;", flat)
    if not loop or not base:
        raise ContractViolation("GetPlayerProgressionFromQuests no longer loops 'BASE + i' over a ProgressionState range")
    states = enum_values(find_enum(src.ip["ip_h"], "ProgressionState", "IndividualProgression.h"))
    lo, hi = states[loop.group(1)], states[loop.group(2)]
    return tuple(int(base.group(1)) + i for i in range(lo, hi + 1))


# --------------------------------------------------------------------------
# Trigger model, evaluated from the pinned RandomPlayerbotMgr::Randomize text
# --------------------------------------------------------------------------

_COND_TOKEN = re.compile(
    r"\s*(\|\||&&|==|!=|<=|>=|<|>|!|\(|\)|\d+|[A-Za-z_][A-Za-z_0-9]*(?:(?:->|\.)[A-Za-z_][A-Za-z_0-9]*(?:\(\))?)*)")
_COND_NAMES = {
    "bot->GetLevel()": "level",
    "bot->getClass()": "cls",
    "CLASS_DEATH_KNIGHT": "CLASS_DEATH_KNIGHT",
    "sPlayerbotAIConfig.RandomBotMaxLevel": "random_bot_max_level",
    "sPlayerbotAIConfig.DowngradeMaxLevelBot": "downgrade_max_level_bot",
}
CLASS_DEATH_KNIGHT = 6


def cpp_condition_to_python(cond: str) -> str:
    """Translate a *restricted* C++ condition to Python; reject anything else."""
    out: List[str] = []
    pos = 0
    cond = cond.strip()
    while pos < len(cond):
        m = _COND_TOKEN.match(cond, pos)
        if not m or m.end() == pos:
            raise ContractViolation(f"unsupported token in randomiser condition at: {cond[pos:]!r}")
        tok = m.group(1)
        pos = m.end()
        if tok == "||":
            out.append(" or ")
        elif tok == "&&":
            out.append(" and ")
        elif tok == "!":
            out.append(" not ")
        elif tok in ("==", "!=", "<=", ">=", "<", ">", "(", ")") or tok.isdigit():
            out.append(f" {tok} ")
        elif tok in _COND_NAMES:
            out.append(f" {_COND_NAMES[tok]} ")
        else:
            raise ContractViolation(f"randomiser condition uses unmodelled symbol {tok!r}")
    return "".join(out)


@dataclass(frozen=True)
class RandomizeDecisionModel:
    in_bg_returns_early: bool
    first_cond: str        # -> RandomizeFirst (full reset)
    second_cond: str       # -> PlayerbotFactory::Randomize(true) (incremental)
    else_full: bool        # else -> RandomizeFirst (full reset)

    def outcome(self, *, level: int, cls: int, random_bot_max_level: int,
                downgrade_max_level_bot: bool, in_battleground: bool = False) -> str:
        if in_battleground and self.in_bg_returns_early:
            return "skipped"
        env = {"level": level, "cls": cls, "CLASS_DEATH_KNIGHT": CLASS_DEATH_KNIGHT,
               "random_bot_max_level": random_bot_max_level,
               "downgrade_max_level_bot": bool(downgrade_max_level_bot)}
        if eval(self.first_cond, {"__builtins__": {}}, env):  # noqa: S307 - restricted tokens only
            return "full_reset"
        if eval(self.second_cond, {"__builtins__": {}}, env):  # noqa: S307
            return "incremental"
        return "full_reset" if self.else_full else "incremental"


def randomize_decision_model(src: Sources) -> RandomizeDecisionModel:
    fn = find_function(src.pb["mgr"], "RandomPlayerbotMgr::Randomize", "RandomPlayerbotMgr.cpp")
    flat = fn.flat
    m = re.fullmatch(
        r"if \(bot->InBattleground\(\)\) return; "
        r"if \((?P<c1>.+?)\) \{ RandomizeFirst\(bot\); \} "
        r"else if \((?P<c2>.+?)\) \{ uint8 level = bot->GetLevel\(\); PlayerbotFactory factory\(bot, level\); "
        r"factory\.Randomize\(true\); \} "
        r"else \{ RandomizeFirst\(bot\); \}", flat)
    if not m:
        raise ContractViolation(
            "RandomPlayerbotMgr::Randomize branch structure changed; re-derive the full-reset trigger matrix.\n"
            f"Body: {flat}")
    return RandomizeDecisionModel(
        in_bg_returns_early=True,
        first_cond=cpp_condition_to_python(m.group("c1")),
        second_cond=cpp_condition_to_python(m.group("c2")),
        else_full=True,
    )


@dataclass(frozen=True)
class PlayerbotsConfigFacts:
    conf_dist: Dict[str, str]
    code_default: Dict[str, str]
    max_level_clamped_to_world_cap: bool


def playerbots_config_facts(src: Sources) -> PlayerbotsConfigFacts:
    keys = ("RandomBotMaxLevel", "DowngradeMaxLevelBot")
    conf: Dict[str, str] = {}
    for k in keys:
        hits = re.findall(r"(?m)^\s*Playerbots\." + k + r"\s*=\s*(\S+)", src.pb["conf_dist"])
        if len(hits) != 1:
            raise ContractViolation(f"playerbots.conf.dist: expected one active Playerbots.{k}, found {hits}")
        conf[k] = hits[0]
    cfg = mask_comments(src.pb["config"])
    code: Dict[str, str] = {}
    for k in keys:
        hits = re.findall(k + r"\s*=\s*sConfigMgr->GetOption<\w+>\(\s*\"Playerbots\." + k + r"\"\s*,\s*([^)]+)\)", cfg)
        if len(hits) != 1:
            raise ContractViolation(f"PlayerbotAIConfig.cpp: expected one load of Playerbots.{k}, found {hits}")
        code[k] = hits[0].strip()
    clamp = bool(re.search(
        r"if \(RandomBotMaxLevel > sWorld->getIntConfig\(CONFIG_MAX_PLAYER_LEVEL\)\) "
        r"RandomBotMaxLevel = sWorld->getIntConfig\(CONFIG_MAX_PLAYER_LEVEL\);", normalize_ws(cfg)))
    return PlayerbotsConfigFacts(conf_dist=conf, code_default=code, max_level_clamped_to_world_cap=clamp)


# --------------------------------------------------------------------------
# Contracts. Each raises ContractViolation on drift.
# --------------------------------------------------------------------------

def _require(cond: bool, msg: str) -> None:
    if not cond:
        raise ContractViolation(msg)


RESET_LOOP_EXPECTED_STATEMENTS = (
    "Quest const* quest = i->second;",
    "uint32 entry = quest->GetQuestId();",
    "if (bot->GetQuestStatus(entry) == QUEST_STATUS_NONE) continue;",
    "bot->RemoveRewardedQuest(entry);",
    "bot->RemoveActiveQuest(entry, false);",
)
MSG_REMOVE_NOT_UNCONDITIONAL = ("ResetQuests: 'bot->RemoveRewardedQuest(entry);' is no longer an unconditional "
                                "top-level statement of the quest-template loop (guarded, wrapped or moved)")


def check_pb_reset_quests_removes_every_non_none_quest(src: Sources) -> None:
    fn = find_function(src.pb["factory"], "PlayerbotFactory::ResetQuests", "PlayerbotFactory.cpp")
    flat = fn.flat
    _require("sObjectMgr->GetQuestTemplates()" in flat, "ResetQuests no longer iterates every loaded quest template")
    _, loop = find_statement(fn, r"\bfor\s*\(\s*ObjectMgr::QuestMap::const_iterator\b")
    _require("questTemplates.begin()" in loop and "questTemplates.end()" in loop,
             "ResetQuests template loop no longer spans questTemplates.begin()..end()")
    stmts = top_level_statements(braced_inner(loop))
    _require("bot->RemoveRewardedQuest(entry);" in stmts, MSG_REMOVE_NOT_UNCONDITIONAL)
    _require(tuple(stmts) == RESET_LOOP_EXPECTED_STATEMENTS,
             "ResetQuests quest-template loop body changed (new filter or statement?): " + " | ".join(stmts))
    big_literals = [int(x) for x in re.findall(r"\b\d{4,}\b", flat)]
    _require(not big_literals, f"ResetQuests now contains ID-like literals {big_literals} (marker exemption?)")


def check_pb_full_randomize_calls_reset_only_when_not_incremental(src: Sources) -> None:
    fn = find_function(src.pb["factory"], "PlayerbotFactory::Randomize", "PlayerbotFactory.cpp")
    calls = re.findall(r"\bResetQuests\s*\(\s*\)\s*;", fn.body)
    _require(len(calls) == 1, f"PlayerbotFactory::Randomize calls ResetQuests {len(calls)} times (expected 1)")
    guarded = [blk for _, blk in find_statements(fn, r"\bif\s*\(\s*!\s*incremental\s*\)")
               if re.search(r"\bResetQuests\s*\(\s*\)\s*;", blk)]
    _require(len(guarded) == 1,
             "ResetQuests is no longer guarded by 'if (!incremental)'; incremental randomise may now wipe markers")


def check_pb_clear_everything_route(src: Sources) -> None:
    fn = find_function(src.pb["factory"], "PlayerbotFactory::ClearEverything", "PlayerbotFactory.cpp")
    _require(re.search(r"^\s*ResetQuests\(\);", fn.body, re.MULTILINE) is not None,
             "ClearEverything no longer calls ResetQuests unconditionally")
    clear = find_function(src.pb["mgr"], "RandomPlayerbotMgr::Clear", "RandomPlayerbotMgr.cpp")
    _require("factory.ClearEverything();" in clear.flat, "RandomPlayerbotMgr::Clear no longer calls ClearEverything")


def check_pb_full_reset_entry_points(src: Sources) -> None:
    first = find_function(src.pb["mgr"], "RandomPlayerbotMgr::RandomizeFirst", "RandomPlayerbotMgr.cpp")
    _require("factory.Randomize(false);" in first.flat, "RandomizeFirst no longer performs Randomize(false)")
    rmin = find_function(src.pb["mgr"], "RandomPlayerbotMgr::RandomizeMin", "RandomPlayerbotMgr.cpp")
    _require("factory.Randomize(false);" in rmin.flat, "RandomizeMin no longer performs Randomize(false)")
    masked = normalize_ws(mask_comments(src.pb["mgr"]))
    live = dict(re.findall(r'handlers\["(\w+)"\] = (?:handlers\["\w+"\] = )?&RandomPlayerbotMgr::(\w+);', masked))
    _require(live.get("init") == "RandomizeFirst", f"console handler 'init' -> {live.get('init')} (expected RandomizeFirst)")
    _require(live.get("clear") == "Clear", f"console handler 'clear' -> {live.get('clear')} (expected Clear)")
    _require("initmin" not in live, "console handler 'initmin' (RandomizeMin) is now live; add it to the route inventory")


def check_pb_trigger_matrix(src: Sources) -> None:
    model = randomize_decision_model(src)
    rows = [
        # level, class, effective RandomBotMaxLevel, DowngradeMaxLevelBot, expected
        (60, 1, 80, False, "incremental"),   # shipped conf.dist values, world cap 80
        (60, 1, 60, False, "incremental"),   # world cap 60, downgrade off
        (60, 1, 60, True, "full_reset"),     # world cap 60 + downgrade on (also code default)
        (59, 1, 60, True, "incremental"),
        (80, 1, 80, True, "full_reset"),
        (2, 1, 80, False, "full_reset"),     # level < 3
        (55, CLASS_DEATH_KNIGHT, 80, False, "full_reset"),
        (56, CLASS_DEATH_KNIGHT, 80, False, "incremental"),
    ]
    for level, cls, max_level, downgrade, expected in rows:
        got = model.outcome(level=level, cls=cls, random_bot_max_level=max_level, downgrade_max_level_bot=downgrade)
        _require(got == expected, f"Randomize matrix drift: level={level} class={cls} max={max_level} "
                                  f"downgrade={downgrade}: got {got}, expected {expected}")
    _require(model.outcome(level=2, cls=1, random_bot_max_level=80, downgrade_max_level_bot=False,
                           in_battleground=True) == "skipped", "in-battleground early return removed")


def check_pb_config_exposure_defaults(src: Sources) -> None:
    facts = playerbots_config_facts(src)
    _require(facts.conf_dist == {"RandomBotMaxLevel": "80", "DowngradeMaxLevelBot": "0"},
             f"playerbots.conf.dist defaults changed: {facts.conf_dist}")
    _require(facts.code_default == {"RandomBotMaxLevel": "80", "DowngradeMaxLevelBot": "true"},
             f"PlayerbotAIConfig code defaults changed: {facts.code_default}")
    _require(facts.max_level_clamped_to_world_cap,
             "RandomBotMaxLevel is no longer clamped to CONFIG_MAX_PLAYER_LEVEL")


def call_site_inventory(src_text: str, class_name: str, callee: str) -> List[str]:
    """Enclosing ``class_name::<fn>`` of every call to ``callee`` (``<outside>`` if none)."""
    masked = mask_comments(src_text)
    spans: List[Tuple[int, int, str]] = []
    for name in sorted(set(re.findall(r"(?m)^[^\n;{}]*?\b" + re.escape(class_name) + r"::(\w+)\s*\(", masked))):
        try:
            fn = find_function(src_text, f"{class_name}::{name}")
        except ExtractionError:
            continue  # a call such as Class::instance(), not a definition
        start = src_text.index(fn.text)
        spans.append((start, start + len(fn.text), name))
    sites: List[str] = []
    for m in re.finditer(r"(?<![\w:])" + re.escape(callee) + r"\s*\(", masked):
        enclosing = [n for a, b, n in spans if a <= m.start() < b]
        sites.append(enclosing[0] if enclosing else "<outside>")
    return sorted(sites)


INITSELF_EXPECTED_QUALITIES = ("ITEM_QUALITY_EPIC", "ITEM_QUALITY_UNCOMMON", "ITEM_QUALITY_RARE",
                               "ITEM_QUALITY_EPIC", "ITEM_QUALITY_LEGENDARY", "ITEM_QUALITY_LEGENDARY, gs")


def check_pb_gm_initself_full_reset_routes(src: Sources) -> None:
    """GM 'initself' variants fully randomise the CALLING player's own character.

    Historical SOURCE risk for a real GM character's saved IP stage; not runtime evidence.
    """
    fn = find_function(src.pb["playerbot_mgr"], "PlayerbotHolder::HandlePlayerbotCommand", "PlayerbotMgr.cpp")
    flat = fn.flat
    for token in ('if (!strcmp(cmd, "initself"))', 'if (!strncmp(cmd, "initself=", 9))',
                  'if (!strcmp(cmd, "initself=uncommon"))', 'if (!strcmp(cmd, "initself=rare"))',
                  'if (!strcmp(cmd, "initself=epic"))', 'if (!strcmp(cmd, "initself=legendary"))',
                  'if (sscanf(cmd, "initself=%d", &gs) != -1)'):
        _require(token in flat, f"HandlePlayerbotCommand no longer dispatches: {token}")
    routes = re.findall(
        r"if \(master->CanBeGameMaster\(\)\) \{ PlayerbotFactory factory\(master, master->GetLevel\(\), "
        r"(ITEM_QUALITY_\w+(?:, gs)?)\); factory\.Randomize\(false\);", flat)
    _require(tuple(routes) == INITSELF_EXPECTED_QUALITIES,
             f"GM initself full-reset routes changed: {routes} (expected {list(INITSELF_EXPECTED_QUALITIES)}, "
             "each gated only by master->CanBeGameMaster())")
    _require(len(re.findall(r"PlayerbotFactory factory\(master\b", flat)) == len(INITSELF_EXPECTED_QUALITIES),
             "HandlePlayerbotCommand builds a PlayerbotFactory on 'master' outside the inventoried initself routes")


LEVEL_MGR_FULL_RESET_FUNCTIONS = ("AdjustBotToRange", "ResetBot", "SkipBotLevel")
LEVEL_MGR_EXPECTED_CALLERS = {
    "AdjustBotToRange": ["ProcessPendingLevelResets"],
    "ResetBot": ["OnBotLevelChanged", "OnBotLevelChanged", "OnBotLogin", "OnBotLogin", "RunResetPlayedTimeCheck"],
    "SkipBotLevel": ["OnBotLevelChanged", "OnBotLogin"],
    "ProcessPendingLevelResets": ["Update"],
    "RunLevelBracketsDistribution": ["Update"],
    "RunResetPlayedTimeCheck": ["Update"],
    "OnBotLogin": ["<outside>"],
    "OnBotLevelChanged": ["<outside>"],
}


def check_pb_level_mgr_full_reset_routes_default_off(src: Sources) -> None:
    """RandomBotLevelMgr brackets / level-reset features fully randomise bots, but are default OFF."""
    text = src.pb["level_mgr"]
    for name in LEVEL_MGR_FULL_RESET_FUNCTIONS:
        fn = find_function(text, f"RandomBotLevelMgr::{name}", "RandomBotLevelMgr.cpp")
        _require("newFactory.Randomize(false);" in fn.flat, f"RandomBotLevelMgr::{name} no longer does Randomize(false)")
    for callee, expected in LEVEL_MGR_EXPECTED_CALLERS.items():
        got = call_site_inventory(text, "RandomBotLevelMgr", callee)
        _require(got == sorted(expected), f"RandomBotLevelMgr call sites of {callee} changed: {got} (expected {sorted(expected)})")
    upd = find_function(text, "RandomBotLevelMgr::Update", "RandomBotLevelMgr.cpp")
    brackets = [b for _, b in find_statements(upd, r"\bif\s*\(\s*sPlayerbotAIConfig\.LevelBracketsEnabled\s*\)")]
    _require(len(brackets) == 1 and "ProcessPendingLevelResets()" in brackets[0]
             and "RunLevelBracketsDistribution()" in brackets[0],
             "Update no longer gates bracket redistribution on LevelBracketsEnabled")
    played = [b for _, b in find_statements(upd, r"\bif\s*\(\s*sPlayerbotAIConfig\.ResetBotLevelEnabled\s*&&")]
    _require(len(played) == 1 and "RunResetPlayedTimeCheck()" in played[0],
             "Update no longer gates the played-time reset on ResetBotLevelEnabled")
    flat_file = normalize_ws(mask_comments(text))
    for hook, call in (("void OnPlayerLogin(Player* player) override", "OnBotLogin(player);"),
                       ("void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override",
                        "OnBotLevelChanged(player, oldLevel);")):
        _require(f"{hook} {{ if (!sPlayerbotAIConfig.ResetBotLevelEnabled) return; RandomBotLevelMgr::instance().{call} }}"
                 in flat_file, f"PlayerScript hook no longer gated on ResetBotLevelEnabled: {hook}")
    cfg = mask_comments(src.pb["config"])
    for member, key in (("LevelBracketsEnabled", "Playerbots.LevelBrackets.Enabled"),
                        ("ResetBotLevelEnabled", "Playerbots.ResetBotLevel.Enabled")):
        _require(re.search(member + r"\s*=\s*sConfigMgr->GetOption<bool>\(\s*\"" + re.escape(key) + r"\"\s*,\s*false\s*\)",
                           cfg) is not None, f"{key} code default is no longer false")
        hits = re.findall(r"(?m)^\s*" + re.escape(key) + r"\s*=\s*(\S+)", src.pb["conf_dist"])
        _require(hits == ["0"], f"playerbots.conf.dist {key} is {hits} (expected ['0'])")


CONFIG_EXPOSURE_KEYS = ("RandomBotMaxLevel", "DowngradeMaxLevelBot")
MIN_PB_SRC_FILES = 500  # the scan below is only meaningful on a full src/ checkout


def check_pb_no_legacy_config_namespace(src: Sources) -> None:
    """At the primary pin no source reads an ``AiPlayerbot.*`` option: old keys are silently ignored."""
    _require(len(src.pb_src_tree) >= MIN_PB_SRC_FILES,
             f"PB src/ scan saw only {len(src.pb_src_tree)} files; check out the whole src/ tree")
    legacy = []
    for rel, text in src.pb_src_tree.items():
        if "AiPlayerbot." in text and re.search(r'"AiPlayerbot\.', mask_comments(text)):
            legacy.append(rel)
    _require(not legacy, f"PB source reads legacy AiPlayerbot.* option keys in {legacy}; re-derive exposure")
    cfg = mask_comments(src.pb["config"])
    for key in CONFIG_EXPOSURE_KEYS:
        _require(len(re.findall(r'"Playerbots\.' + key + r'"', cfg)) == 1,
                 f"Playerbots.{key} is not read exactly once in PlayerbotAIConfig.cpp")


def legacy_config_facts(config_cpp: str, conf_dist: str) -> Dict[str, Tuple[str, str, str]]:
    """{key: (namespace, code default, conf.dist value)} for the exposure keys of any PB revision."""
    cfg = mask_comments(config_cpp)
    out: Dict[str, Tuple[str, str, str]] = {}
    for key in CONFIG_EXPOSURE_KEYS:
        hits = re.findall(r'GetOption<\w+>\(\s*"(\w+)\.' + key + r'"\s*,\s*([^)]+)\)', cfg)
        if len(hits) != 1:
            raise ContractViolation(f"expected one load of *.{key}, found {hits}")
        ns, default = hits[0]
        dist = re.findall(r"(?m)^\s*" + re.escape(ns) + r"\." + key + r"\s*=\s*(\S+)", conf_dist)
        if len(dist) != 1:
            raise ContractViolation(f"conf.dist: expected one active {ns}.{key}, found {dist}")
        out[key] = (ns, default.strip(), dist[0])
    return out


def check_ip_level_caps_are_xp_gates_not_world_cap(src: Sources) -> None:
    """IP's Vanilla/bot caps stop XP; they do not set worldserver MaxPlayerLevel (the PB clamp input)."""
    users = [rel for rel, text in src.ip_src_tree.items()
             if "CONFIG_MAX_PLAYER_LEVEL" in mask_comments(text)]
    _require(not users, f"IP source now references CONFIG_MAX_PLAYER_LEVEL in {users}; re-derive the cap premise")
    _require(re.search(r'BotAccountsMaxLevel = sConfigMgr->GetOption<uint8>\("IndividualProgression\.BotAccountsMaxLevel", 80\);',
                       normalize_ws(mask_comments(src.ip["ip_cpp"]))) is not None,
             "IndividualProgression.BotAccountsMaxLevel load/default (80) changed")
    give = find_function(src.ip["ip_player"], "OnPlayerGiveXP", "IndividualProgressionPlayer.cpp").flat
    _require("if (player->GetLevel() >= sIndividualProgression->BotAccountsMaxLevel)" in give and "amount = 0;" in give,
             "OnPlayerGiveXP no longer caps bot XP at BotAccountsMaxLevel")
    _require("if (!sIndividualProgression->hasPassedProgression(player, PROGRESSION_PRE_TBC) && player->GetLevel() >= 60)"
             in give, "OnPlayerGiveXP no longer stops pre-TBC (stage < 8) human XP at level 60")


def check_core_get_quest_status_rewarded_semantics(src: Sources) -> None:
    fn = find_function(src.core["player_quest"], "Player::GetQuestStatus", "PlayerQuest.cpp")
    flat = fn.flat
    order = [
        "QuestStatusMap::const_iterator itr = m_QuestStatus.find(quest_id);",
        "return itr->second.Status;",
        "if (qInfo->IsSeasonal())",
        "if (!qInfo->IsRepeatable() && IsQuestRewarded(quest_id))",
        "return QUEST_STATUS_REWARDED;",
        "return QUEST_STATUS_NONE;",
    ]
    pos = -1
    for piece in order:
        nxt = flat.find(piece, pos + 1)
        _require(nxt > pos, f"Player::GetQuestStatus no longer contains (in order): {piece}")
        pos = nxt
    rep = find_function(src.core["quest_def_h"], "IsRepeatable", "QuestDef.h")
    _require(rep.flat == "return SpecialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE;",
             f"Quest::IsRepeatable changed: {rep.flat}")
    consts = core_quest_constants(src)
    _require(consts.repeatable == 0x0001, f"QUEST_SPECIAL_FLAGS_REPEATABLE is {consts.repeatable:#x}")
    inline = find_function(src.core["player_h"], "IsQuestRewarded", "Player.h")
    _require(inline.flat == "return m_RewardedQuests.find(quest_id) != m_RewardedQuests.end();",
             f"Player::IsQuestRewarded changed: {inline.flat}")


def check_core_reward_and_remove_bookkeeping(src: Sources) -> None:
    rem = find_function(src.core["player_quest"], "Player::RemoveRewardedQuest", "PlayerQuest.cpp").flat
    _require("m_RewardedQuests.erase(rewItr);" in rem, "RemoveRewardedQuest no longer erases from m_RewardedQuests")
    _require("m_RewardedQuestsSave[questId] = false;" in rem,
             "RemoveRewardedQuest no longer queues a DB delete (m_RewardedQuestsSave[questId] = false)")
    setr = find_function(src.core["player_quest"], "Player::SetRewardedQuest", "PlayerQuest.cpp").flat
    _require(setr == "m_RewardedQuests.insert(quest_id); m_RewardedQuestsSave[quest_id] = true;",
             f"Player::SetRewardedQuest changed: {setr}")
    rq = find_function(src.core["player_quest"], "Player::RewardQuest", "PlayerQuest.cpp").flat
    _require(re.search(r"RemoveActiveQuest\(quest_id, false\); SetRewardedQuest\(quest_id\);", rq) is not None,
             "RewardQuest no longer ends with RemoveActiveQuest + SetRewardedQuest (marker status-map assumption)")


def check_core_save_persists_rewarded_delete(src: Sources) -> None:
    fn = find_function(src.core["player_storage"], "Player::_SaveQuestStatus", "PlayerStorage.cpp")
    _, loop = find_statement(fn, r"\bfor\s*\(\s*saveItr\s*=\s*m_RewardedQuestsSave\.begin\(\)")
    flat = normalize_ws(loop)
    _require(re.search(r"if \(saveItr->second\) stmt = CharacterDatabase\.GetPreparedStatement\(CHAR_INS_CHAR_QUESTSTATUS_REWARDED\); "
                       r"else stmt = CharacterDatabase\.GetPreparedStatement\(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST\);",
                       flat) is not None,
             "_SaveQuestStatus no longer maps m_RewardedQuestsSave=false to CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST")
    _require("trans->Append(stmt);" in flat, "_SaveQuestStatus rewarded loop no longer appends to the transaction")


def world_select_columns(object_mgr_src: str, table: str) -> List[str]:
    """Columns of the single ``WorldDatabase.Query("SELECT ... FROM <table>...")``."""
    masked = mask_comments(object_mgr_src)
    found: List[List[str]] = []
    for m in re.finditer(r"WorldDatabase\.Query\(", masked):
        close = match_bracket(masked, m.end() - 1)
        sql = concatenated_string_literal(masked[m.end():close])
        sel = re.match(r"\s*SELECT\s+(.*?)\s+FROM\s+" + re.escape(table) + r"(?:\s|$)", sql, re.DOTALL)
        if sel:
            found.append([c.strip() for c in sel.group(1).split(",")])
    if len(found) != 1:
        raise ContractViolation(f"expected one WorldDatabase SELECT ... FROM {table}, found {len(found)}")
    return found[0]


def check_core_loader_column_mapping(src: Sources) -> None:
    cols = world_select_columns(src.core["object_mgr"], "quest_template")
    _require(len(cols) > 22 and cols[4] == "QuestSortID" and cols[22] == "Flags",
             f"quest_template SELECT columns moved: [4]={cols[4]!r} [22]={cols[22] if len(cols) > 22 else None!r}")
    qd = mask_comments(src.core["quest_def_cpp"])
    _require("ZoneOrSort = questRecord[4].Get<int16>();" in qd, "Quest ctor no longer reads ZoneOrSort from column 4")
    _require("Flags = questRecord[22].Get<uint32>();" in qd, "Quest ctor no longer reads Flags from column 22")
    acols = world_select_columns(src.core["object_mgr"], "quest_template_addon")
    _require(len(acols) > 18 and acols[18] == "SpecialFlags",
             f"quest_template_addon SELECT column 18 is {acols[18] if len(acols) > 18 else None!r}")
    _require("SpecialFlags = fields[18].Get<uint32>();" in qd, "addon loader no longer reads SpecialFlags from field 18")


def check_core_loader_normalisation(src: Sources) -> None:
    om = normalize_ws(mask_comments(src.core["object_mgr"]))
    for snippet in (
        "if (qinfo->SpecialFlags & ~QUEST_SPECIAL_FLAGS_DB_ALLOWED)",
        "qinfo->SpecialFlags &= QUEST_SPECIAL_FLAGS_DB_ALLOWED;",
        "if (qinfo->Flags & QUEST_FLAGS_DAILY && qinfo->Flags & QUEST_FLAGS_WEEKLY)",
        "if (qinfo->Flags & QUEST_FLAGS_DAILY) { if (!(qinfo->SpecialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE))",
        "if (qinfo->Flags & QUEST_FLAGS_WEEKLY) { if (!(qinfo->SpecialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE))",
        "if (qinfo->SpecialFlags & QUEST_SPECIAL_FLAGS_MONTHLY) { if (!(qinfo->SpecialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE))",
    ):
        _require(snippet in om, f"ObjectMgr::LoadQuests normalisation changed; missing: {snippet}")
    _require(om.count("qinfo->SpecialFlags |= QUEST_SPECIAL_FLAGS_REPEATABLE;") == 3,
             "ObjectMgr::LoadQuests no longer forces REPEATABLE in exactly the daily/weekly/monthly cases")
    consts = core_quest_constants(src)
    _require("QUEST_SPECIAL_FLAGS_REPEATABLE" in consts.db_allowed_mask_names,
             "QUEST_SPECIAL_FLAGS_DB_ALLOWED composition could not be read")


def check_ip_sql_markers_are_rewarded_status_quests(src: Sources) -> None:
    templates = ip_marker_templates(src)
    _require(tuple(sorted(templates)) == EXPECTED_STAGE_MARKERS,
             f"player_progression.sql stage markers are {sorted(templates)}, expected 66001..66018")
    consts = core_quest_constants(src)
    for qid, t in templates.items():
        _require(t.special_flags == 256, f"{qid}: SpecialFlags={t.special_flags} (expected 256)")
        _require(not t.is_repeatable(consts.repeatable),
                 f"{qid}: effective SpecialFlags {t.effective_special_flags:#x} is REPEATABLE; "
                 "GetQuestStatus would hide the marker from IP itself")
        _require(not t.is_seasonal(consts),
                 f"{qid}: QuestSortID {t.quest_sort_id} makes the marker seasonal (status from calendar, not reward)")


def check_ip_reads_exactly_the_sql_markers(src: Sources) -> None:
    ids = ip_progression_quest_ids(src)
    _require(ids == EXPECTED_STAGE_MARKERS, f"IP reads marker IDs {ids[0]}..{ids[-1]}, expected 66001..66018")
    flat = find_function(src.ip["ip_cpp"], "IndividualProgression::GetPlayerProgressionFromQuests",
                         "IndividualProgression.cpp").flat
    _require("if (player->GetQuestStatus(PROGRESSION_QUEST) == QUEST_STATUS_REWARDED)" in flat,
             "IP no longer derives stage from GetQuestStatus(...) == QUEST_STATUS_REWARDED")


def check_no_other_ip_sql_redefines_markers(src: Sources) -> None:
    findings: List[str] = []
    for rel, text in src.ip_sql_tree.items():
        if rel == IP_FILES["progression_sql"]:
            continue
        for hit in statements_touching_ids(text, ("quest_template", "quest_template_addon"), EXPECTED_STAGE_MARKERS):
            findings.append(f"{rel}: {hit}")
    _require(not findings, "other IP SQL touches stage-marker quest rows: " + "; ".join(findings))


def check_chain_verdict_markers_deleted_by_full_reset(src: Sources) -> None:
    """Compose the individually verified links into one per-marker verdict."""
    consts = core_quest_constants(src)
    for t in ip_marker_templates(src).values():
        # Mirrors the verified Player::GetQuestStatus order for a quest with no
        # m_QuestStatus entry (RewardQuest removes it): seasonal -> calendar,
        # else !repeatable && rewarded -> REWARDED, else NONE.
        if t.is_seasonal(consts):
            status_when_rewarded = "calendar-dependent"
        elif not t.is_repeatable(consts.repeatable):
            status_when_rewarded = "QUEST_STATUS_REWARDED"
        else:
            status_when_rewarded = "QUEST_STATUS_NONE"
        _require(status_when_rewarded == "QUEST_STATUS_REWARDED",
                 f"{t.quest_id}: rewarded marker would read as {status_when_rewarded}")
    # ResetQuests skips only QUEST_STATUS_NONE (verified above), so every
    # rewarded stage marker is removed and the delete is persisted on save.


ALL_CHECKS: Tuple[Callable[[Sources], None], ...] = (
    check_pb_reset_quests_removes_every_non_none_quest,
    check_pb_full_randomize_calls_reset_only_when_not_incremental,
    check_pb_clear_everything_route,
    check_pb_full_reset_entry_points,
    check_pb_trigger_matrix,
    check_pb_config_exposure_defaults,
    check_pb_gm_initself_full_reset_routes,
    check_pb_level_mgr_full_reset_routes_default_off,
    check_pb_no_legacy_config_namespace,
    check_core_get_quest_status_rewarded_semantics,
    check_core_reward_and_remove_bookkeeping,
    check_core_save_persists_rewarded_delete,
    check_core_loader_column_mapping,
    check_core_loader_normalisation,
    check_ip_sql_markers_are_rewarded_status_quests,
    check_ip_reads_exactly_the_sql_markers,
    check_no_other_ip_sql_redefines_markers,
    check_ip_level_caps_are_xp_gates_not_world_cap,
    check_chain_verdict_markers_deleted_by_full_reset,
)


def run_all(src: Sources) -> Dict[str, Optional[str]]:
    """Map check name -> None (pass) or failure message."""
    results: Dict[str, Optional[str]] = {}
    for check in ALL_CHECKS:
        try:
            check(src)
            results[check.__name__] = None
        except ContractViolation as exc:
            results[check.__name__] = str(exc)
    return results


# --------------------------------------------------------------------------
# Compiled extracted-body harness generator
# --------------------------------------------------------------------------

RESET_SKIP_IP_MARKERS_MUTATION = "reset_skip_ip_markers"
RESET_SKIP_TRACKING_FLAG_MUTATION = "reset_skip_tracking_flag"
COMPILED_MUTATIONS = (RESET_SKIP_IP_MARKERS_MUTATION, RESET_SKIP_TRACKING_FLAG_MUTATION)


def _banner(origin: str, sha: str, path: str, line: int) -> str:
    return f"// ---- VERBATIM from {origin}@{sha} {path}:{line} ----\n"


def generate_harness_sources(src: Sources, out_dir: Path, mutations: Sequence[str] = ()) -> List[Path]:
    """Write the pinned_*.inc files consumed by tests/marker_reset_harness."""
    unknown = set(mutations) - set(COMPILED_MUTATIONS)
    if unknown:
        raise ValueError(f"unknown compiled mutations {sorted(unknown)}")
    out_dir.mkdir(parents=True, exist_ok=True)
    core, pb, ip = src.core, src.pb, src.ip
    cs, ps = PINNED_CORE_SHA, PINNED_PLAYERBOTS_SHA

    decls = [
        "// Generated by tests/ip_marker_reset_extract.py. DO NOT EDIT. Test-only.\n",
        f"// Core {cs}\n" + find_define(core["quest_def_h"], "MAX_QUEST_LOG_SIZE") + "\n",
        find_enum(core["quest_def_h"], "QuestStatus", "QuestDef.h") + "\n",
        find_enum(core["quest_def_h"], "QuestSpecialFlags", "QuestDef.h") + "\n",
        find_enum(core["quest_def_h"], "QuestFlags", "QuestDef.h") + "\n",
        find_enum(core["shared_defines"], "QuestSort", "SharedDefines.h") + "\n",
        "// IP fork under test\n" + find_enum(ip["ip_h"], "ProgressionState", "IndividualProgression.h") + "\n",
    ]
    quest_methods = []
    for name in ("GetQuestId", "HasFlag", "IsRepeatable", "IsSeasonal"):
        fn = find_function(core["quest_def_h"], name, "QuestDef.h")
        quest_methods.append(_banner("Core", cs, CORE_FILES["quest_def_h"], fn.line) + fn.text.strip() + "\n")
    player_inline = []
    fn = find_function(core["player_h"], "IsQuestRewarded", "Player.h")
    player_inline.append(_banner("Core", cs, CORE_FILES["player_h"], fn.line) + fn.text.strip() + "\n")

    bodies: List[str] = ["// Generated by tests/ip_marker_reset_extract.py. DO NOT EDIT. Test-only.\n"]
    for name in ("Player::GetQuestStatus", "Player::RemoveActiveQuest", "Player::RemoveRewardedQuest",
                 "Player::SetRewardedQuest"):
        fn = find_function(core["player_quest"], name, "PlayerQuest.cpp")
        bodies.append(_banner("Core", cs, CORE_FILES["player_quest"], fn.line) + fn.text + "\n")
    save_fn = find_function(core["player_storage"], "Player::_SaveQuestStatus", "PlayerStorage.cpp")
    loop, _ = find_statement(save_fn, r"\bfor\s*\(\s*saveItr\s*=\s*m_RewardedQuestsSave\.begin\(\)")
    bodies.append(
        _banner("Core", cs, CORE_FILES["player_storage"], save_fn.line) +
        "// Only the rewarded-quest persistence loop of Player::_SaveQuestStatus is spliced here.\n"
        "void Player::HarnessSaveRewardedQuests(CharacterDatabaseTransaction trans)\n{\n"
        "    QuestStatusSaveMap::iterator saveItr;\n"
        "    CharacterDatabasePreparedStatement* stmt = nullptr;\n    " + loop + "\n"
        "    m_RewardedQuestsSave.clear();\n}\n")
    reset = find_function(pb["factory"], "PlayerbotFactory::ResetQuests", "PlayerbotFactory.cpp")
    reset_text = reset.text
    if RESET_SKIP_IP_MARKERS_MUTATION in mutations:
        anchor = "uint32 entry = quest->GetQuestId();"
        if reset_text.count(anchor) != 1:
            raise ExtractionError("mutation anchor missing in ResetQuests")
        reset_text = reset_text.replace(
            anchor, anchor + "\n        if (entry >= 66001 && entry <= 66018) continue; // NEGATIVE FIXTURE ONLY")
    if RESET_SKIP_TRACKING_FLAG_MUTATION in mutations:
        anchor = "bot->RemoveRewardedQuest(entry);"
        if reset_text.count(anchor) != 1:
            raise ExtractionError("mutation anchor missing in ResetQuests")
        reset_text = reset_text.replace(
            anchor, "if (!quest->HasFlag(QUEST_FLAGS_TRACKING)) // NEGATIVE FIXTURE ONLY\n            " + anchor)
    bodies.append(_banner("mod-playerbots", ps, PB_FILES["factory"], reset.line) + reset_text + "\n")
    ipfn = find_function(ip["ip_cpp"], "IndividualProgression::GetPlayerProgressionFromQuests", "IndividualProgression.cpp")
    bodies.append(_banner("IP", "under-test", IP_FILES["ip_cpp"], ipfn.line) + ipfn.text + "\n")

    templates = ip_marker_templates(src)
    pvp = ip_marker_templates(src, PVP_MARKER_RANGE)
    rows = ["// Generated from data/sql/world/base/player_progression.sql. DO NOT EDIT.\n",
            "// {quest_id, QuestSortID, Flags, raw SpecialFlags, Core-loader effective SpecialFlags}\n",
            "static const HarnessMarkerRow kStageMarkers[] = {\n"]
    rows += [f"    {{{t.quest_id}, {t.quest_sort_id}, {t.flags}u, {t.special_flags}u, {t.effective_special_flags}u}},\n"
             for t in templates.values()]
    rows.append("};\nstatic const HarnessMarkerRow kPvpMarkers[] = {\n")
    rows += [f"    {{{t.quest_id}, {t.quest_sort_id}, {t.flags}u, {t.special_flags}u, {t.effective_special_flags}u}},\n"
             for t in pvp.values()]
    rows.append("};\n")
    rows.append(f'static const char* const kHarnessMutations = "{",".join(mutations)}";\n')

    files = {
        "pinned_decls.inc": "".join(decls),
        "pinned_quest_methods.inc": "".join(quest_methods),
        "pinned_player_inline.inc": "".join(player_inline),
        "pinned_bodies.inc": "".join(bodies),
        "pinned_sql_markers.inc": "".join(rows),
    }
    written = []
    for name, text in files.items():
        p = out_dir / name
        p.write_text(text, encoding="utf-8")
        written.append(p)
    return written


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate", help="write pinned_*.inc for the compiled harness")
    g.add_argument("--out", required=True, type=Path)
    g.add_argument("--mutation", action="append", default=[], choices=COMPILED_MUTATIONS)
    sub.add_parser("check", help="run every source contract and print a summary")
    args = ap.parse_args(argv)
    src = load_sources()
    if args.cmd == "generate":
        for p in generate_harness_sources(src, args.out, args.mutation):
            print(p)
        return 0
    results = run_all(src)
    for name, err in results.items():
        print(("PASS " if err is None else "FAIL ") + name + ("" if err is None else f": {err}"))
    return 0 if all(v is None for v in results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
