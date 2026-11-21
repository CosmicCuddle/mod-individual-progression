"""Compile actual social-reply marker helpers against Core's real DataMap.

Player is a test double: this verifies the extracted helper implementation,
not native PlayerScript registration, map ownership or Worldserver gameplay.
Example: python tests/run_f14_reply_session_adapter_test.py --core ../core --compiler clang++
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--compiler", nargs="+", default=["clang++"])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    core_include = args.core.resolve() / "src/common/Utilities"
    if not (core_include / "DataMap.h").is_file():
        parser.error("--core must contain src/common/Utilities/DataMap.h")
    script = (root / "src/IndividualProgressionF14SocialIsolationScript.cpp").read_text(encoding="utf-8")
    start = script.index('    constexpr char const* ReplySessionKey =')
    end = script.index('    std::mutex& PendingRepliesLock()', start)
    helpers = script[start:end]
    harness = r'''
#include "IndividualProgressionF14SocialIsolationPolicy.h"
#include "DataMap.h"
#include <cstdio>
using uint64 = std::uint64_t;
namespace Policy = IndividualProgressionF14SocialIsolationPolicy;
struct Player { DataMap CustomData; };
'''+helpers+r'''
#define CHECK(value) do { ++checks; if (!(value)) { std::printf("FAIL line %d\n", __LINE__); return 1; } } while (0)
int main()
{
    unsigned checks = 0;
    Player original;
    CHECK(ReadReplySession(nullptr) == 0);
    BindReplySession(nullptr);
    InvalidateReplySession(nullptr);
    CHECK(ReadReplySession(&original) == 0);
    CHECK(original.CustomData.Get<ReplySessionMarker>(ReplySessionKey) == nullptr);
    BindReplySession(&original);
    auto const initial = ReadReplySession(&original);
    CHECK(initial != 0);
    CHECK(Policy::ReplyEpochsMatch(initial, ReadReplySession(&original)));
    InvalidateReplySession(&original); // disconnect/logout
    CHECK(ReadReplySession(&original) == 0);
    CHECK(!Policy::ReplyEpochsMatch(initial, ReadReplySession(&original)));
    BindReplySession(&original); // reconnect retaining Player
    auto const reconnect = ReadReplySession(&original);
    CHECK(reconnect != 0 && reconnect != initial);
    CHECK(!Policy::ReplyEpochsMatch(initial, reconnect));
    BindReplySession(&original); // repeated rebind is a fresh identity
    CHECK(!Policy::ReplyEpochsMatch(reconnect, ReadReplySession(&original)));
    Player replacement; // full relog: a different Player with the same character GUID
    BindReplySession(&replacement);
    auto const relog = ReadReplySession(&replacement);
    CHECK(relog != 0 && relog != ReadReplySession(&original));
    InvalidateReplySession(&original); // late departure of old object
    CHECK(ReadReplySession(&replacement) == relog);
    CHECK(!Policy::ReplyEpochsMatch(initial, relog));
    CHECK(!Policy::ReplyEpochsMatch(0, relog));
    std::printf("F14 EXTRACTED REPLY HELPERS %u/%u passed (Player test double; real Core DataMap)\n", checks, checks);
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="f14-reply-session-") as tmp:
        source = Path(tmp) / "reply-session.cpp"
        binary = Path(tmp) / "reply-session.exe"
        source.write_text(harness, encoding="utf-8")
        command = args.compiler + ["-std=c++20", "-pthread", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                   "-O2", "-I" + str(root / "src"), "-I" + str(core_include),
                                   str(source), "-o", str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
