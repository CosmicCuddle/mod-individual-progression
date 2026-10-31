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


# --- Marker-table SQL static gate --------------------------------------------
# A narrow, documented STATIC gate, explicitly NOT a SQL engine and NOT a game test.
# It reports any statement in other IP SQL files that could change a column the
# marker chain depends on, for a stage-marker row (66001..66018), or that it cannot
# decide (fail closed). PvP markers 66101..66114 are deliberately NOT stage markers.

CHAIN_COLUMNS = {
    "quest_template": frozenset({"id", "flags", "questsortid"}),
    "quest_template_addon": frozenset({"id", "specialflags"}),
}
MARKER_TABLES = tuple(CHAIN_COLUMNS)
_DDL_VERBS = ("TRUNCATE", "DROP", "ALTER", "RENAME")
_STORED_OR_DYNAMIC_SQL_KEYWORDS = frozenset({"CREATE", "PREPARE", "TRIGGER", "PROCEDURE"})
# Revision 5 (CS02-IP F1, self-review S1/S2): verbs allowed to NAME a marker table without analysis.
# Every other unmodelled verb (LOAD DATA/XML, a leading WITH ... UPDATE, future syntax) fails closed.
_READ_ONLY_VERBS = frozenset({"SELECT", "SET", "SHOW", "HANDLER", "DESCRIBE", "DESC", "EXPLAIN"})
_WRITE_VERBS = ("INSERT", "REPLACE", "UPDATE", "DELETE")
_EXPLAIN_VERBS = frozenset({"EXPLAIN", "DESCRIBE", "DESC"})
_EXECUTABLE_COMMENT_RE = re.compile(r"/\*!(?P<body>.*?)\*/", re.DOTALL)

_SQL_TOKEN_RE = re.compile(r"""\s*(?:
    (?P<str>'(?:[^'\\]|\\.|'')*'|"(?:[^"\\]|\\.|"")*")
  | (?P<num>\d+(?:\.\d+)?)
  | (?P<ident>(?:`[^`]+`|[A-Za-z_@][\w$@]*)(?:\s*\.\s*(?:`[^`]+`|[A-Za-z_][\w$]*))*)
  | (?P<op><=|>=|<>|!=|:=|\|\||&&|[=<>(),;*+\-/&|^~%.])
)""", re.VERBOSE)


class _Undecidable(Exception):
    """The static gate cannot decide this predicate (caller fails closed)."""


def sql_tokens(stmt: str) -> Optional[List[Tuple[str, str]]]:
    """Quote-aware tokens [(kind, text)]; None if the text cannot be tokenised."""
    out: List[Tuple[str, str]] = []
    pos, n = 0, len(stmt)
    while pos < n:
        if stmt[pos:].strip() == "":
            break
        m = _SQL_TOKEN_RE.match(stmt, pos)
        if not m or m.end() == pos:
            return None
        kind = m.lastgroup
        out.append((kind, m.group(kind)))
        pos = m.end()
    return out


def _ident_name(text: str) -> str:
    """`db`.`tbl` / q.`ID` / Id -> last part, lower case, unquoted."""
    return re.split(r"\s*\.\s*", text)[-1].strip("`").lower()


def _is_kw(tok: Tuple[str, str], *words: str) -> bool:
    return tok[0] == "ident" and tok[1].upper() in words


def _top_level_index(tokens: Sequence[Tuple[str, str]], *words: str, start: int = 0) -> int:
    depth = 0
    for i in range(start, len(tokens)):
        kind, text = tokens[i]
        if text == "(":
            depth += 1
        elif text == ")":
            depth -= 1
        elif depth == 0 and _is_kw(tokens[i], *words):
            return i
    return -1


def _split_top_level(tokens: Sequence[Tuple[str, str]], sep: str) -> List[List[Tuple[str, str]]]:
    parts: List[List[Tuple[str, str]]] = [[]]
    depth = 0
    for tok in tokens:
        if tok[1] == "(":
            depth += 1
        elif tok[1] == ")":
            depth -= 1
        if depth == 0 and tok[1] == sep:
            parts.append([])
        else:
            parts[-1].append(tok)
    return parts


class _WhereParser:
    """Recursive descent over a deliberately tiny predicate grammar (see module docs)."""

    def __init__(self, tokens: Sequence[Tuple[str, str]]):
        self.t = [tok for tok in tokens if tok[1] != ";"]
        self.i = 0

    def peek(self, k: int = 0) -> Optional[Tuple[str, str]]:
        return self.t[self.i + k] if self.i + k < len(self.t) else None

    def take(self) -> Tuple[str, str]:
        tok = self.peek()
        if tok is None:
            raise _Undecidable("unexpected end of WHERE")
        self.i += 1
        return tok

    def parse(self):
        node = self.expr()
        if self.peek() is not None:
            raise _Undecidable(f"unsupported WHERE syntax near {self.peek()[1]!r}")
        return node

    def expr(self):
        nodes = [self.term()]
        while self.peek() and (_is_kw(self.peek(), "OR") or self.peek()[1] == "||"):
            self.take()
            nodes.append(self.term())
        return ("or", nodes) if len(nodes) > 1 else nodes[0]

    def term(self):
        nodes = [self.factor()]
        while self.peek() and (_is_kw(self.peek(), "AND") or self.peek()[1] == "&&"):
            self.take()
            nodes.append(self.factor())
        return ("and", nodes) if len(nodes) > 1 else nodes[0]

    def factor(self):
        tok = self.peek()
        if tok and _is_kw(tok, "NOT"):
            self.take()
            return ("not", self.factor())
        if tok and tok[1] == "(":
            self.take()
            node = self.expr()
            if self.take()[1] != ")":
                raise _Undecidable("unbalanced parenthesis")
            return node
        return self.predicate()

    def operand(self):
        kind, text = self.take()
        if kind == "num":
            return ("lit", int(text) if text.isdigit() else float(text))
        if kind == "op" and text == "-" and self.peek() and self.peek()[0] == "num":
            num = self.take()[1]
            return ("lit", -(int(num) if num.isdigit() else float(num)))
        if kind == "str":
            return ("lit", text[1:-1].replace(text[0] * 2, text[0]))
        if kind == "ident" and text.upper() not in ("SELECT", "NULL", "AND", "OR", "NOT", "IN", "BETWEEN", "IS"):
            return ("col", _ident_name(text))
        raise _Undecidable(f"unsupported operand {text!r}")

    def predicate(self):
        left = self.operand()
        tok = self.peek()
        if tok is None:
            raise _Undecidable("bare operand is not a predicate")
        negate = False
        if _is_kw(tok, "NOT"):
            negate = True
            self.take()
            tok = self.peek()
        if tok and tok[0] == "op" and tok[1] in ("=", "!=", "<>", "<", ">", "<=", ">=") and not negate:
            op = self.take()[1]
            return ("cmp", op, left, self.operand())
        if tok and _is_kw(tok, "IN"):
            self.take()
            if self.take()[1] != "(":
                raise _Undecidable("IN without a literal list")
            vals = [self.operand()]
            while self.peek() and self.peek()[1] == ",":
                self.take()
                vals.append(self.operand())
            if self.take()[1] != ")" or any(v[0] != "lit" for v in vals):
                raise _Undecidable("IN list is not literals only (subquery?)")
            node = ("in", left, [v[1] for v in vals])
            return ("not", node) if negate else node
        if tok and _is_kw(tok, "BETWEEN"):
            self.take()
            lo = self.operand()
            if not _is_kw(self.take(), "AND"):
                raise _Undecidable("BETWEEN without AND")
            node = ("between", left, lo, self.operand())
            return ("not", node) if negate else node
        if tok and _is_kw(tok, "IS") and not negate:
            self.take()
            neg = False
            if self.peek() and _is_kw(self.peek(), "NOT"):
                self.take()
                neg = True
            if not _is_kw(self.take(), "NULL"):
                raise _Undecidable("IS without NULL")
            node = ("isnull", left)
            return ("not", node) if neg else node
        raise _Undecidable(f"unsupported predicate operator {tok[1]!r}")


def _value(node, row: Dict[str, object]):
    if node[0] == "lit":
        return node[1]
    return row.get(node[1], _UNKNOWN)


_UNKNOWN = object()


def _cmp(op: str, a, b) -> Optional[bool]:
    if a is _UNKNOWN or b is _UNKNOWN or a is None or b is None:
        return None
    if isinstance(a, str) and isinstance(b, str):
        a, b = a.casefold(), b.casefold()
    elif isinstance(a, str) or isinstance(b, str):
        return None  # mixed string/number comparison: MySQL coercion is not modelled
    return {"=": a == b, "!=": a != b, "<>": a != b, "<": a < b, ">": a > b, "<=": a <= b, ">=": a >= b}[op]


def eval_where(node, row: Dict[str, object]) -> Optional[bool]:
    """Kleene three-valued evaluation of a parsed WHERE against one row (None = unknown)."""
    kind = node[0]
    if kind in ("and", "or"):
        vals = [eval_where(n, row) for n in node[1]]
        if kind == "and":
            return False if False in vals else (None if None in vals else True)
        return True if True in vals else (None if None in vals else False)
    if kind == "not":
        v = eval_where(node[1], row)
        return None if v is None else not v
    if kind == "cmp":
        return _cmp(node[1], _value(node[2], row), _value(node[3], row))
    if kind == "in":
        vals = [_cmp("=", _value(node[1], row), v) for v in node[2]]
        return True if True in vals else (None if None in vals else False)
    if kind == "between":
        lo, hi = _cmp(">=", _value(node[1], row), _value(node[2], row)), _cmp("<=", _value(node[1], row), _value(node[3], row))
        return eval_where(("and", [("const", lo), ("const", hi)]), row)
    if kind == "const":
        return node[1]
    if kind == "isnull":
        v = _value(node[1], row)
        return None if v is _UNKNOWN else v is None
    raise _Undecidable(kind)


def _where_findings(where_tokens, rows: Sequence[Dict[str, object]], what: str) -> List[str]:
    try:
        tree = _WhereParser(where_tokens).parse()
        results = [(row.get("id"), eval_where(tree, row)) for row in rows]
    except _Undecidable as exc:
        return [f"{what}: WHERE not statically decidable ({exc}) (fail closed)"]
    hit = sorted(i for i, r in results if r is True)
    unknown = sorted(i for i, r in results if r is None)
    out = []
    if hit:
        out.append(f"{what}: WHERE matches stage-marker row(s) {hit[0]}..{hit[-1]} ({len(hit)})")
    if unknown:
        out.append(f"{what}: WHERE undecidable for stage-marker row(s) {unknown[0]}..{unknown[-1]} (fail closed)")
    return out


def _skip_modifiers(tokens, i: int, *mods: str) -> int:
    while i < len(tokens) and _is_kw(tokens[i], *mods):
        i += 1
    return i


def _odku_index(toks) -> int:
    """Token index of a top-level ON DUPLICATE KEY UPDATE, or -1."""
    i = _top_level_index(toks, "ON")
    while i >= 0:
        if i + 3 < len(toks) and _is_kw(toks[i + 1], "DUPLICATE") and _is_kw(toks[i + 2], "KEY") \
                and _is_kw(toks[i + 3], "UPDATE"):
            return i
        i = _top_level_index(toks, "ON", start=i + 1)
    return -1


def _strip_odku(stmt: str) -> str:
    matches = list(re.finditer(r"\bON\s+DUPLICATE\s+KEY\s+UPDATE\b", stmt, re.IGNORECASE))
    return stmt[:matches[-1].start()] if matches else stmt


def _odku_findings(assign_tokens, table: str, short: str) -> List[str]:
    written = []
    for assign in _split_top_level(assign_tokens, ","):
        assign = [t for t in assign if t[1] != ";"]
        if len(assign) < 3 or assign[0][0] != "ident" or assign[1][1] != "=":
            return [f"{table}: UNPARSEABLE ON DUPLICATE KEY UPDATE list (fail closed): {short}"]
        col = _ident_name(assign[0][1])
        written.append(col)
        if col == "id":
            value = assign[2:]
            if len(value) == 1 and value[0][0] == "num" and value[0][1].isdigit():
                if int(value[0][1]) in EXPECTED_STAGE_MARKERS:
                    return [f"{table}: ON DUPLICATE KEY UPDATE assigns ID = {value[0][1]} (a stage-marker ID) "
                            f"to an existing row: {short}"]
            else:
                return [f"{table}: ON DUPLICATE KEY UPDATE assigns ID from an expression (fail closed): {short}"]
    chain = sorted(set(written).intersection(CHAIN_COLUMNS[table]) - {"id"})
    if chain:
        return [f"{table}: ON DUPLICATE KEY UPDATE writes chain column(s) {chain} on whichever row already "
                f"holds the key (fail closed): {short}"]
    return []


def _analyse_marker_statement(stmt: str, rows_by_table: Dict[str, Tuple[Dict[str, object], ...]]) -> List[str]:
    toks = sql_tokens(stmt)
    if toks is None:
        return [f"UNPARSEABLE statement naming a marker table (fail closed): {normalize_ws(stmt)[:80]}"]
    verb = toks[0][1].upper() if toks and toks[0][0] == "ident" else ""
    short = normalize_ws(stmt)[:80]
    keywords = {t[1].upper() for t in toks if t[0] == "ident"}
    if keywords.intersection(_STORED_OR_DYNAMIC_SQL_KEYWORDS):
        # N2: triggers/procedures/views/events and dynamic SQL that NAME a marker table (even
        # inside a string literal) cannot be analysed statically -> fail closed.
        return [f"{'/'.join(sorted(keywords.intersection(_STORED_OR_DYNAMIC_SQL_KEYWORDS)))} naming a marker "
                f"table (stored/dynamic SQL, fail closed): {short}"]
    # Under sql_mode=ANSI_QUOTES a double-quoted token is an IDENTIFIER. Only tokens whose content IS
    # a marker-table name count (e.g. "quest_template", "world"."quest_template"); ordinary
    # double-quoted text values such as LogDescription = "Collect ..." never match.
    dq_markers = [t[1] for t in toks if t[0] == "str" and t[1].startswith('"')
                  and t[1][1:-1].strip().lower() in MARKER_TABLES]
    names = {_ident_name(t[1]) for t in toks if t[0] == "ident"}
    if verb in _EXPLAIN_VERBS and (dq_markers or names.intersection(MARKER_TABLES)):
        # Revision 6 (CS02-IP G2): EXPLAIN ANALYZE executes the statement on some engines; an
        # EXPLAIN of a write verb is not a plain read either. Engine/version unverified -> fail closed.
        k = 1
        if k + 2 < len(toks) and _is_kw(toks[k], "FORMAT") and toks[k + 1][1] == "=":
            k += 3
        if k < len(toks) and (_is_kw(toks[k], "ANALYZE") or _is_kw(toks[k], *_WRITE_VERBS)):
            return [f"{verb} with ANALYZE or a write verb on a marker table (engine behaviour unverified) "
                    f"(fail closed): {short}"]
    if dq_markers and verb not in _READ_ONLY_VERBS:
        # Revision 6 (CS02-IP G1): anywhere in the statement, not only the first write target.
        return [f"double-quoted marker-table identifier {dq_markers[0]} in {verb or toks[0][1]} (ANSI_QUOTES; "
                f"effective SQL mode unverified) (fail closed): {short}"]
    if not names.intersection(MARKER_TABLES):
        return []

    if verb in _DDL_VERBS:
        return [f"{verb} on a marker table (fail closed): {short}"]

    if verb in ("INSERT", "REPLACE"):
        i = _skip_modifiers(toks, 1, "LOW_PRIORITY", "DELAYED", "HIGH_PRIORITY", "IGNORE", "INTO")
        if i >= len(toks) or _ident_name(toks[i][1]) not in MARKER_TABLES:
            return []  # target is another table (a marker table only appears in a sub-select)
        table = _ident_name(toks[i][1])
        head = stmt
        odku_at = _odku_index(toks)
        if odku_at >= 0:
            # Revision 5 (CS02-IP F2): ON DUPLICATE KEY UPDATE rewrites an EXISTING row, whatever
            # the VALUES say. Check its assignments; parse rows only from the part before it.
            odku = _odku_findings(toks[odku_at + 4:], table, short)
            if odku:
                return odku
            head = _strip_odku(stmt)
        ins = parse_insert(head)
        if ins is not None:
            id_cols = [c for c in ins.columns if c.lower() == "id"]
            if len(id_cols) != 1:
                return [f"{table}: INSERT/REPLACE without exactly one ID column (fail closed): {short}"]
            ids = [r[id_cols[0]] for r in ins.rows]
            if any(not isinstance(v, int) for v in ids):
                return [f"{table}: INSERT/REPLACE with a non-literal ID (fail closed): {short}"]
            stage = sorted(set(ids).intersection(EXPECTED_STAGE_MARKERS))
            return [f"{table}: INSERT/REPLACE of stage-marker ID(s) {stage}"] if stage else []
        pos = _POSITIONAL_INSERT_RE.match(head)
        if pos:
            ids = [row[0] if row else None for row in _value_tuples(pos.group(2))]
            if not ids or any(not isinstance(v, int) for v in ids):
                return [f"{table}: column-less INSERT/REPLACE with a non-literal ID (fail closed): {short}"]
            stage = sorted(set(ids).intersection(EXPECTED_STAGE_MARKERS))
            return [f"{table}: column-less INSERT/REPLACE of stage-marker ID(s) {stage}"] if stage else []
        return [f"UNPARSEABLE insert on a marker table (fail closed): {short}"]

    if verb == "UPDATE":
        set_at = _top_level_index(toks, "SET")
        if set_at < 0:
            return [f"UNPARSEABLE UPDATE on a marker table (no SET) (fail closed): {short}"]
        target = toks[_skip_modifiers(toks, 1, "LOW_PRIORITY", "IGNORE"):set_at]
        target_tables = sorted({_ident_name(t[1]) for t in target if t[0] == "ident"}.intersection(MARKER_TABLES))
        if not target_tables:
            return []  # marker table only read in a sub-select
        where_at = _top_level_index(toks, "WHERE", start=set_at)
        tail_at = _top_level_index(toks, "ORDER", "LIMIT", start=set_at)
        set_end = min(x for x in (where_at, tail_at, len(toks)) if x >= 0)
        written = []
        for assign in _split_top_level(toks[set_at + 1:set_end], ","):
            assign = [t for t in assign if t[1] != ";"]
            if len(assign) < 3 or assign[0][0] != "ident" or assign[1][1] != "=":
                return [f"UNPARSEABLE SET list on a marker table (fail closed): {short}"]
            written.append(_ident_name(assign[0][1]))
            if written[-1] == "id":
                value = assign[2:]
                if len(value) == 1 and value[0][0] == "num" and value[0][1].isdigit():
                    if int(value[0][1]) in EXPECTED_STAGE_MARKERS:
                        return [f"UPDATE {target_tables[0]}: SET ID = {value[0][1]} assigns a stage-marker ID "
                                f"to another row: {short}"]
                else:
                    return [f"UPDATE {target_tables[0]}: SET ID from an expression could produce a stage-marker "
                            f"ID (fail closed): {short}"]
        relevant = sorted({c for c in written if any(c in CHAIN_COLUMNS[t] for t in target_tables)})
        if not relevant:
            return []  # documented allowlist: writes no chain-relevant column
        single = (len(target) in (1, 2) or (len(target) == 3 and _is_kw(target[1], "AS"))) and \
            _ident_name(target[0][1]) in MARKER_TABLES
        what = f"UPDATE {target_tables[0]} SET {relevant}"
        if not single:
            return [f"multi-table UPDATE writing chain column(s) {relevant} on {target_tables} (fail closed): {short}"]
        if where_at < 0:
            return [f"{what}: no WHERE, rewrites every row including stage markers"]
        where_end = tail_at if tail_at > where_at else len(toks)
        return _where_findings(toks[where_at + 1:where_end], rows_by_table[target_tables[0]], what)

    if verb == "DELETE":
        i = _skip_modifiers(toks, 1, "LOW_PRIORITY", "QUICK", "IGNORE")
        if not (i + 1 < len(toks) and _is_kw(toks[i], "FROM") and _ident_name(toks[i + 1][1]) in MARKER_TABLES):
            from_at = _top_level_index(toks, "FROM")
            using = toks[from_at + 1:] if from_at >= 0 else toks
            if {_ident_name(t[1]) for t in using[:3] if t[0] == "ident"}.intersection(MARKER_TABLES) or from_at < 0:
                return [f"multi-table or unparseable DELETE on a marker table (fail closed): {short}"]
            return []
        table = _ident_name(toks[i + 1][1])
        rest = [t for t in toks[i + 2:] if t[1] != ";"]
        what = f"DELETE FROM {table}"
        if not rest:
            return [f"{what}: no WHERE, deletes every row including stage markers"]
        if not _is_kw(rest[0], "WHERE"):
            return [f"{what}: unsupported clause {rest[0][1]!r} (fail closed)"]
        tail_at = _top_level_index(rest, "ORDER", "LIMIT")
        return _where_findings(rest[1:tail_at if tail_at > 0 else len(rest)], rows_by_table[table], what)

    if verb in _READ_ONLY_VERBS:
        return []  # documented read-only allowlist
    return [f"unsupported statement form '{verb or toks[0][1]}' naming a marker table (only INSERT/REPLACE/UPDATE/"
            f"DELETE are analysed; fail closed): {short}"]


_MARKER_TABLE_TEXT_RE = re.compile(r"\b(?:quest_template|quest_template_addon)\b", re.IGNORECASE)


@functools.lru_cache(maxsize=2048)
def _marker_sql_findings_cached(sql_text: str, rows_key: Tuple) -> Tuple[str, ...]:
    rows_by_table = {t: tuple(dict(r) for r in rows) for t, rows in rows_key}
    if not _MARKER_TABLE_TEXT_RE.search(sql_text):
        return ()
    hits: List[str] = []
    for m in _EXECUTABLE_COMMENT_RE.finditer(sql_text):
        if _MARKER_TABLE_TEXT_RE.search(m.group("body")):
            # N2: MySQL runs /*! ... */ bodies, but iter_sql_statements drops comments -> fail closed.
            hits.append(f"MySQL executable comment naming a marker table (fail closed): "
                        f"{normalize_ws(m.group(0))[:80]}")
    for stmt in iter_sql_statements(sql_text):
        if _MARKER_TABLE_TEXT_RE.search(stmt):
            hits.extend(_analyse_marker_statement(stmt, rows_by_table))
    return tuple(hits)


def _rows_key(rows_by_table: Dict[str, Tuple[Dict[str, object], ...]]) -> Tuple:
    return tuple((t, tuple(tuple(sorted(r.items())) for r in rows)) for t, rows in sorted(rows_by_table.items()))


def marker_sql_statement_findings(sql_text: str, rows_by_table: Dict[str, Tuple[Dict[str, object], ...]]) -> List[str]:
    """Per-statement static-gate findings for one SQL text (no cross-statement analysis)."""
    return list(_marker_sql_findings_cached(sql_text, _rows_key(rows_by_table)))


def marker_sql_findings(sql_text: str, rows_by_table: Dict[str, Tuple[Dict[str, object], ...]]) -> List[str]:
    """Static-gate findings for one SQL text: per-statement findings plus the cross-statement
    dependency findings WITHIN that text (see marker_sql_dependency_findings)."""
    return (marker_sql_statement_findings(sql_text, rows_by_table)
            + marker_sql_dependency_findings({"<input>": sql_text}, rows_by_table))


# --- Cross-statement / cross-file dependency (revision 4, CS02-IP N1) -------------------------
# The per-statement gate evaluates each WHERE against the ORIGINAL marker rows. A statement that
# writes only non-chain columns is allowlisted, so a LATER chain write whose WHERE filters on a
# column that the earlier statement changed was invisible (same file or another file). This is a
# bounded, conservative two-pass analysis, NOT statement execution:
#   pass A (fixpoint): an UPDATE on a marker table "may touch stage-marker rows" if its WHERE is
#     absent, TRUE/undecidable for any of the 18 rows, or references a column that ANOTHER
#     touching UPDATE writes. Its SET columns become MUTABLE for that table.
#   pass B: a chain-writing UPDATE, or any DELETE, whose WHERE references a column that ANOTHER
#     touching UPDATE writes is a finding (fail closed). Order is deliberately ignored.

@dataclass(frozen=True)
class _MarkerWrite:
    key: Tuple[str, int]
    verb: str
    tables: Tuple[str, ...]
    set_cols: frozenset
    where_state: str           # "absent" | "parsed" | "undecidable"
    where_tree: object
    where_cols: frozenset
    text: str


def _where_columns(node) -> set:
    if not isinstance(node, tuple):
        return set()
    if node[0] == "col":
        return {node[1]}
    out: set = set()
    for part in node[1:]:
        if isinstance(part, list):
            for p in part:
                out |= _where_columns(p)
        else:
            out |= _where_columns(part)
    return out


def _parse_where(tokens) -> Tuple[str, object, frozenset]:
    try:
        tree = _WhereParser(tokens).parse()
    except _Undecidable:
        return "undecidable", None, frozenset()
    return "parsed", tree, frozenset(_where_columns(tree))


def _marker_write(stmt: str, key: Tuple[str, int]) -> Optional[_MarkerWrite]:
    """UPDATE/DELETE on a marker table as a read/write summary (None for anything else)."""
    toks = sql_tokens(stmt)
    if not toks or toks[0][0] != "ident":
        return None
    verb = toks[0][1].upper()
    short = normalize_ws(stmt)[:70]
    if verb == "UPDATE":
        set_at = _top_level_index(toks, "SET")
        if set_at < 0:
            return None
        target = toks[_skip_modifiers(toks, 1, "LOW_PRIORITY", "IGNORE"):set_at]
        tables = tuple(sorted({_ident_name(t[1]) for t in target if t[0] == "ident"}.intersection(MARKER_TABLES)))
        if not tables:
            return None
        where_at = _top_level_index(toks, "WHERE", start=set_at)
        tail_at = _top_level_index(toks, "ORDER", "LIMIT", start=set_at)
        set_end = min(x for x in (where_at, tail_at, len(toks)) if x >= 0)
        cols = set()
        for assign in _split_top_level(toks[set_at + 1:set_end], ","):
            assign = [t for t in assign if t[1] != ";"]
            if assign and assign[0][0] == "ident":
                cols.add(_ident_name(assign[0][1]))
        if where_at < 0:
            state, tree, wcols = "absent", None, frozenset()
        else:
            where_end = tail_at if tail_at > where_at else len(toks)
            state, tree, wcols = _parse_where([t for t in toks[where_at + 1:where_end] if t[1] != ";"])
        return _MarkerWrite(key, verb, tables, frozenset(cols), state, tree, wcols, short)
    if verb == "DELETE":
        i = _skip_modifiers(toks, 1, "LOW_PRIORITY", "QUICK", "IGNORE")
        if not (i + 1 < len(toks) and _is_kw(toks[i], "FROM") and _ident_name(toks[i + 1][1]) in MARKER_TABLES):
            return None  # multi-table DELETE already fails closed per statement
        rest = [t for t in toks[i + 2:] if t[1] != ";"]
        if not rest or not _is_kw(rest[0], "WHERE"):
            state, tree, wcols = "absent", None, frozenset()
        else:
            tail_at = _top_level_index(rest, "ORDER", "LIMIT")
            state, tree, wcols = _parse_where(rest[1:tail_at if tail_at > 0 else len(rest)])
        return _MarkerWrite(key, verb, (_ident_name(toks[i + 1][1]),), frozenset(), state, tree, wcols, short)
    return None


@functools.lru_cache(maxsize=1024)
def _file_marker_writes(rel: str, text: str) -> Tuple[_MarkerWrite, ...]:
    if not _MARKER_TABLE_TEXT_RE.search(text):
        return ()
    out = []
    for n, stmt in enumerate(iter_sql_statements(text), 1):
        if _MARKER_TABLE_TEXT_RE.search(stmt):
            w = _marker_write(stmt, (rel, n))
            if w is not None:
                out.append(w)
    return tuple(out)


def _touches_original_rows(w: _MarkerWrite, rows_by_table) -> bool:
    if w.where_state != "parsed":
        return True
    for t in w.tables:
        for row in rows_by_table[t]:
            if eval_where(w.where_tree, row) in (True, None):
                return True
    return False


def marker_sql_dependency_findings(files: Dict[str, str],
                                   rows_by_table: Dict[str, Tuple[Dict[str, object], ...]]) -> List[str]:
    """Cross-statement AND cross-file dependency findings over ``files`` (see block comment)."""
    writes: List[Tuple[str, _MarkerWrite]] = []
    for rel, text in files.items():
        writes.extend((rel, w) for w in _file_marker_writes(rel, text))
    original_touch = {w.key: _touches_original_rows(w, rows_by_table) for _, w in writes if w.verb == "UPDATE"}
    mutators: Dict[Tuple[str, str], List[Tuple[str, _MarkerWrite]]] = {}

    def others(w: _MarkerWrite, table: str, col: str):
        return [(r, m) for r, m in mutators.get((table, col), []) if m.key != w.key]

    touching = set()
    changed = True
    while changed:  # pass A fixpoint (monotone, bounded by statements x columns)
        changed = False
        for rel, w in writes:
            if w.verb != "UPDATE" or w.key in touching:
                continue
            dep = any(others(w, t, c) for t in w.tables for c in w.where_cols)
            if dep or original_touch[w.key]:
                touching.add(w.key)
                for t in w.tables:
                    for c in w.set_cols:
                        mutators.setdefault((t, c), []).append((rel, w))
                changed = True
    findings: List[str] = []
    for rel, w in writes:
        chain = w.verb == "DELETE" or any(c in CHAIN_COLUMNS[t] for t in w.tables for c in w.set_cols)
        if not chain or w.where_state != "parsed":
            continue  # absent/undecidable WHEREs on chain writes already fail closed per statement
        for t in w.tables:
            for c in sorted(w.where_cols):
                src = others(w, t, c)
                if src:
                    mrel, m = src[0]
                    more = f" and {len(src) - 1} other statement(s)" if len(src) > 1 else ""
                    findings.append(
                        f"{rel}: {w.verb} {t} (statement #{w.key[1]}): WHERE depends on column '{c}', which "
                        f"{mrel} statement #{m.key[1]} ('{m.text}'){more} may change on stage-marker rows "
                        f"(cross-statement dependency, fail closed)")
    return findings


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


def ip_marker_rows(src: Sources) -> Dict[str, Tuple[Dict[str, object], ...]]:
    """Stage-marker rows (66001..66018) of both marker tables from player_progression.sql,
    keys lower-cased. Used by the SQL static gate to evaluate WHERE clauses."""
    rows: Dict[str, List[Dict[str, object]]] = {t: [] for t in MARKER_TABLES}
    for stmt in iter_sql_statements(src.ip["progression_sql"]):
        ins = parse_insert(stmt)
        if ins is None or ins.table.lower() not in rows:
            continue
        for r in ins.rows:
            low = {k.lower(): v for k, v in r.items()}
            if low.get("id") in EXPECTED_STAGE_MARKERS:
                rows[ins.table.lower()].append(low)
    for t, rs in rows.items():
        if sorted(r["id"] for r in rs) != list(EXPECTED_STAGE_MARKERS):
            raise ContractViolation(f"player_progression.sql: {t} stage-marker rows are incomplete")
    return {t: tuple(rs) for t, rs in rows.items()}


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
RESET_FUNCTION_EXPECTED_STATEMENTS = (
    "for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot) { bot->SetQuestSlot(slot, 0); }",
    "ObjectMgr::QuestMap const& questTemplates = sObjectMgr->GetQuestTemplates();",
    "for (ObjectMgr::QuestMap::const_iterator i = questTemplates.begin(); i != questTemplates.end(); ++i) {",
)
MSG_RESET_FUNCTION_CHANGED = ("ResetQuests: function body changed outside the quest-template loop "
                              "(early return, extra loop or post-loop restore?)")
MSG_REMOVE_NOT_UNCONDITIONAL = ("ResetQuests: 'bot->RemoveRewardedQuest(entry);' is no longer an unconditional "
                                "top-level statement of the quest-template loop (guarded, wrapped or moved)")


def check_pb_reset_quests_removes_every_non_none_quest(src: Sources) -> None:
    fn = find_function(src.pb["factory"], "PlayerbotFactory::ResetQuests", "PlayerbotFactory.cpp")
    flat = fn.flat
    _require("sObjectMgr->GetQuestTemplates()" in flat, "ResetQuests no longer iterates every loaded quest template")
    # CS02-IP rev-2 Low: pin the function's own top-level statements too (exactly three).
    fn_stmts = top_level_statements(fn.body)
    _require(len(fn_stmts) == 3 and fn_stmts[0] == RESET_FUNCTION_EXPECTED_STATEMENTS[0]
             and fn_stmts[1] == RESET_FUNCTION_EXPECTED_STATEMENTS[1]
             and fn_stmts[2].startswith(RESET_FUNCTION_EXPECTED_STATEMENTS[2]), MSG_RESET_FUNCTION_CHANGED)
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
    routes = [quality for _, quality in re.findall(
        r"if \(master->CanBeGameMaster\(\)\) \{ PlayerbotFactory (\w+)\s*\(\s*master, master->GetLevel\(\), "
        r"(ITEM_QUALITY_\w+(?:, gs)?)\); \1\.Randomize\(false\);", flat)]
    _require(tuple(routes) == INITSELF_EXPECTED_QUALITIES,
             f"GM initself full-reset routes changed: {routes} (expected {list(INITSELF_EXPECTED_QUALITIES)}, "
             "each gated only by master->CanBeGameMaster())")
    _require(len(re.findall(r"PlayerbotFactory\s+\w+\s*\(\s*master\b", flat)) == len(INITSELF_EXPECTED_QUALITIES),
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


# --- Tree-wide full-reset call-site inventory (CS02-IP rev-2 Low; optional rev-3 item) -------
# Every call of these callees anywhere under PB src/ is mapped to (file, enclosing qualified
# function or "<none>", callee) and the multiset must equal the frozen inventory below. Header
# DECLARATIONS appear as "<none>" entries on purpose: any new caller, in any file, changes the
# multiset. Only meaningful at the frozen pin (a new pin requires re-deriving this table).
PB_FULL_RESET_CALLEES = {
    "Randomize(false)": r"\b\w+\s*(?:\.|->)\s*Randomize\s*\(\s*false\s*\)",
    "Randomize(<player>)": r"(?<![\w:&.>])Randomize\s*\(\s*(?!false\b|true\b)[A-Za-z_]\w*\s*\)",
    "sRandomPlayerbotMgr.Randomize(": r"\bsRandomPlayerbotMgr\s*(?:\.|->)\s*Randomize\s*\(",
    "RandomizeFirst(": r"(?<![\w:&])RandomizeFirst\s*\(",
    "RandomizeMin(": r"(?<![\w:&])RandomizeMin\s*\(",
    "ClearEverything(": r"(?<![\w:&])ClearEverything\s*\(",
}
EXPECTED_PB_FULL_RESET_CALL_INVENTORY = {
    ("src/Bot/Factory/PlayerbotFactory.h", "<none>", "ClearEverything("): 1,
    ("src/Bot/PlayerbotMgr.cpp", "PlayerbotHolder::HandlePlayerbotCommand", "Randomize(false)"): 6,
    ("src/Bot/PlayerbotMgr.cpp", "PlayerbotHolder::OnBotLogin", "Randomize(false)"): 1,
    ("src/Bot/PlayerbotMgr.cpp", "PlayerbotHolder::ProcessBotCommand", "Randomize(false)"): 7,
    ("src/Bot/PlayerbotMgr.cpp", "PlayerbotHolder::ProcessBotCommand", "sRandomPlayerbotMgr.Randomize("): 1,
    ("src/Bot/RandomBotLevelMgr.cpp", "RandomBotLevelMgr::AdjustBotToRange", "Randomize(false)"): 1,
    ("src/Bot/RandomBotLevelMgr.cpp", "RandomBotLevelMgr::ResetBot", "Randomize(false)"): 1,
    ("src/Bot/RandomBotLevelMgr.cpp", "RandomBotLevelMgr::SkipBotLevel", "Randomize(false)"): 1,
    ("src/Bot/RandomPlayerbotMgr.cpp", "RandomPlayerbotMgr::Clear", "ClearEverything("): 1,
    ("src/Bot/RandomPlayerbotMgr.cpp", "RandomPlayerbotMgr::ProcessBot", "Randomize(<player>)"): 1,
    ("src/Bot/RandomPlayerbotMgr.cpp", "RandomPlayerbotMgr::Randomize", "RandomizeFirst("): 2,
    ("src/Bot/RandomPlayerbotMgr.cpp", "RandomPlayerbotMgr::RandomizeFirst", "Randomize(false)"): 1,
    ("src/Bot/RandomPlayerbotMgr.cpp", "RandomPlayerbotMgr::RandomizeMin", "Randomize(false)"): 1,
    ("src/Bot/RandomPlayerbotMgr.h", "<none>", "RandomizeFirst("): 1,
    ("src/Bot/RandomPlayerbotMgr.h", "<none>", "RandomizeMin("): 1,
}


def definition_spans(text: str) -> List[Tuple[int, int, str]]:
    """(start, end, qualified name) of every out-of-class definition; overloads allowed."""
    masked = mask_comments(text)
    spans: List[Tuple[int, int, str]] = []
    for d in re.finditer(r"(?m)^[^\n;{}]*?(?<![\w:])((?:\w+::)+\w+)\s*\(", masked):
        try:
            close = match_bracket(masked, d.end() - 1)
        except ExtractionError:
            continue
        tail = re.match(r"\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?\{", masked[close + 1:])
        if tail:
            body_open = close + tail.end()
            spans.append((d.start(), match_bracket(masked, body_open), d.group(1)))
    return spans


def pb_call_inventory(src: Sources, callees: Dict[str, str], needles: Tuple[str, ...]) -> Dict[Tuple[str, str, str], int]:
    """(file, enclosing qualified function or <none>, callee) -> count over the whole PB src/ tree."""
    inv: Dict[Tuple[str, str, str], int] = {}
    for rel, text in src.pb_src_tree.items():
        if not any(n in text for n in needles):
            continue
        masked = mask_comments(text)
        hits = [(name, m.start()) for name, pat in callees.items() for m in re.finditer(pat, masked)]
        if not hits:
            continue
        spans = definition_spans(text)
        for name, pos in hits:
            enclosing = [n for a, b, n in spans if a <= pos <= b]
            key = (rel, enclosing[-1] if enclosing else "<none>", name)
            inv[key] = inv.get(key, 0) + 1
    return inv


def pb_full_reset_call_inventory(src: Sources) -> Dict[Tuple[str, str, str], int]:
    return pb_call_inventory(src, PB_FULL_RESET_CALLEES, ("Randomize", "ClearEverything"))


def check_pb_tree_wide_full_reset_call_inventory(src: Sources) -> None:
    _require(len(src.pb_src_tree) >= MIN_PB_SRC_FILES,
             f"PB src/ scan saw only {len(src.pb_src_tree)} files; check out the whole src/ tree")
    got = pb_full_reset_call_inventory(src)
    added = {k: v for k, v in got.items() if EXPECTED_PB_FULL_RESET_CALL_INVENTORY.get(k) != v}
    missing = {k: v for k, v in EXPECTED_PB_FULL_RESET_CALL_INVENTORY.items() if got.get(k) != v}
    _require(not added and not missing,
             f"PB full-reset call-site inventory changed: new/changed {added}; missing/changed {missing}")


# --- Rewarded-removal callers (revision 4 optional; CS02-IP Option-A review finding) -----------
# Every PB call of RemoveRewardedQuest, wherever it is. Only PlayerbotFactory::ResetQuests is a
# full reset; DropQuestAction::Execute ('drop <quest link>') can remove ONE rewarded marker without
# any reset. Frozen at e88cc409. Stage IDs 66001-66018 and PvP rank IDs 66101-66114 are both
# reachable by ID-agnostic removal; no preservation policy for either set is implied here.
PB_REWARDED_REMOVAL_CALLEES = {"RemoveRewardedQuest(": r"(?:\.|->)\s*RemoveRewardedQuest\s*\("}
EXPECTED_PB_REWARDED_REMOVAL_INVENTORY = {
    ("src/Ai/Base/Actions/DropQuestAction.cpp", "CleanQuestLogAction::DropQuestType", "RemoveRewardedQuest("): 1,
    ("src/Ai/Base/Actions/DropQuestAction.cpp", "CleanQuestLogAction::Execute", "RemoveRewardedQuest("): 1,
    ("src/Ai/Base/Actions/DropQuestAction.cpp", "DropQuestAction::Execute", "RemoveRewardedQuest("): 1,
    ("src/Bot/Factory/PlayerbotFactory.cpp", "PlayerbotFactory::ResetQuests", "RemoveRewardedQuest("): 1,
}


def check_pb_rewarded_removal_caller_inventory(src: Sources) -> None:
    _require(len(src.pb_src_tree) >= MIN_PB_SRC_FILES,
             f"PB src/ scan saw only {len(src.pb_src_tree)} files; check out the whole src/ tree")
    got = pb_call_inventory(src, PB_REWARDED_REMOVAL_CALLEES, ("RemoveRewardedQuest",))
    added = {k: v for k, v in got.items() if EXPECTED_PB_REWARDED_REMOVAL_INVENTORY.get(k) != v}
    missing = {k: v for k, v in EXPECTED_PB_REWARDED_REMOVAL_INVENTORY.items() if got.get(k) != v}
    _require(not added and not missing,
             f"PB RemoveRewardedQuest caller inventory changed: new/changed {added}; missing/changed {missing}")


DROP_QUEST_EXPECTED_SEQUENCE = (
    "uint32 entry = handler.extractQuestId(link);",
    "for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot) {",
    "if (!entry) return false;",
    "bot->RemoveRewardedQuest(entry);",
    "bot->RemoveActiveQuest(entry, false);",
)
MSG_DROP_QUEST_CHANGED = ("DropQuestAction::Execute: 'drop <quest link>' control flow changed (expected: entry from "
                          "the link, slot loop only overrides entry on a match, then unconditional "
                          "RemoveRewardedQuest(entry) for any non-zero entry)")


def check_pb_drop_quest_removes_rewarded_marker_without_log_slot(src: Sources) -> None:
    """Master 'drop <quest link>': a non-zero quest ID with NO matching log slot still reaches
    RemoveRewardedQuest(entry). [SRC] control flow only; hidden-link chat reachability UNTESTED."""
    fn = find_function(src.pb_src_tree.get("src/Ai/Base/Actions/DropQuestAction.cpp", ""),
                       "DropQuestAction::Execute", "DropQuestAction.cpp")
    stmts = top_level_statements(fn.body)
    try:
        i = stmts.index(DROP_QUEST_EXPECTED_SEQUENCE[0])
    except ValueError:
        raise ContractViolation(MSG_DROP_QUEST_CHANGED)
    window = stmts[i:i + len(DROP_QUEST_EXPECTED_SEQUENCE)]
    ok = (len(window) == len(DROP_QUEST_EXPECTED_SEQUENCE)
          and window[1].startswith(DROP_QUEST_EXPECTED_SEQUENCE[1])
          and all(a == b for a, b in zip(window[2:], DROP_QUEST_EXPECTED_SEQUENCE[2:])))
    _require(ok, MSG_DROP_QUEST_CHANGED)
    loop_assignments = re.findall(r"\bentry\s*=(?!=)[^;]*;", window[1])
    _require(loop_assignments == ["entry = logQuest;"], MSG_DROP_QUEST_CHANGED)
    _require(not re.findall(r"\b\d{4,}\b", fn.flat), MSG_DROP_QUEST_CHANGED + " (ID-like literal added)")


def drop_quest_affected_marker_sets(src: Sources) -> Dict[str, Tuple[int, ...]]:
    """Derived from the contract above: removal is ID-agnostic, so both sets are reachable."""
    check_pb_drop_quest_removes_rewarded_marker_without_log_slot(src)
    return {"stage": EXPECTED_STAGE_MARKERS, "pvp_rank": tuple(PVP_MARKER_RANGE)}


ADDCLASS_LOGIN_CONDITION = "addClassBot && master && abs((int)master->GetLevel() - (int)bot->GetLevel()) > 3"


def addclass_login_full_reset(account_type: int, master_present: bool, master_level: int, bot_level: int,
                              src: Sources) -> bool:
    """Evaluate the pinned OnBotLogin AddClass auto-randomise condition (asserted verbatim first)."""
    check_pb_addclass_login_full_reset_route(src)
    return (account_type == 2) and master_present and abs(master_level - bot_level) > 3


def check_pb_addclass_login_full_reset_route(src: Sources) -> None:
    """AddClass (account type 2) bots with a master and a level gap > 3 are fully randomised in
    PlayerbotHolder::OnBotLogin (after Core login). Source route only; owner reports no AddClass use."""
    flat = find_function(src.pb["playerbot_mgr"], "PlayerbotHolder::OnBotLogin", "PlayerbotMgr.cpp").flat
    _require("bool addClassBot = sRandomPlayerbotMgr.IsAccountType(accountId, 2);" in flat,
             "OnBotLogin AddClass classification changed (expected IsAccountType(accountId, 2))")
    m = re.search(r"if \((addClassBot && master && .+?)\) \{ uint32 mixedGearScore = .*? "
                  r"PlayerbotFactory (\w+)\(bot, master->GetLevel\(\), ITEM_QUALITY_LEGENDARY, mixedGearScore\); "
                  r"\2\.Randomize\(false\); \}", flat)
    _require(m is not None and m.group(1) == ADDCLASS_LOGIN_CONDITION,
             f"OnBotLogin AddClass full-randomise condition changed: {m.group(1) if m else None!r}")


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
    """Static gate: no other IP SQL statement may change a chain column of a stage-marker row,
    or be undecidable while writing one (fail closed). NOT a SQL engine; see marker_sql_findings."""
    rows = ip_marker_rows(src)
    others = {rel: text for rel, text in src.ip_sql_tree.items() if rel != IP_FILES["progression_sql"]}
    findings: List[str] = []
    for rel, text in others.items():
        findings.extend(f"{rel}: {hit}" for hit in marker_sql_statement_findings(text, rows))
    # Revision 4 (CS02-IP N1): whole-tree pass, so a dependency split across two files is seen too.
    findings.extend(marker_sql_dependency_findings(others, rows))
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
    check_pb_tree_wide_full_reset_call_inventory,
    check_pb_addclass_login_full_reset_route,
    check_pb_rewarded_removal_caller_inventory,
    check_pb_drop_quest_removes_rewarded_marker_without_log_slot,
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
