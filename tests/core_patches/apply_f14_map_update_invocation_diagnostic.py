#!/usr/bin/env python3
"""Disposable, pinned Core-only F14 Map::Update invocation diagnostics.

Add a thread-local, strictly scoped Map::Update invocation marker. This proves
only that the current call stack is inside Map::Update for this exact Map.
It is NOT proof of exclusive map ownership or 3.3.5a client visibility.
No actual server, Core fork, gameplay policy, IP quest state or packet is changed.
"""
from pathlib import Path
import sys

FILES = (
    "src/server/game/Maps/Map.h",
    "src/server/game/Maps/Map.cpp",
)

def replace_once(source, old, new, label):
    count = source.count(old)
    if count != 1:
        raise ValueError(f"{label}: expected one anchor, got {count}; refusing patch")
    return source.replace(old, new, 1)

def edit_map_h(source):
    old = "    virtual void Update(const uint32, const uint32, bool thread = true);"
    new = old + """

    // F14 disposable diagnostics only: true only within this Map's current
    // Map::Update call stack. NOT an exclusive map-thread or client ACK proof.
    [[nodiscard]] bool IsWithinF14MapUpdateInvocation() const;
"""
    return replace_once(source, old, new, "Map.h public method")

def edit_map_cpp(source):
    old = "void Map::Update(const uint32 t_diff, const uint32 s_diff, bool  /*thread*/)\n{"
    new = """// F14 disposable diagnostics only. A worker may run DIFFERENT maps on
// different requests; thread-id alone is never a map-ownership token.
// A nested map invocation restores its preceding frame, including on unwind.
namespace
{
    thread_local Map const* f14CurrentMapUpdateInvocation = nullptr;

    class F14ScopedMapUpdateInvocation final
    {
    public:
        explicit F14ScopedMapUpdateInvocation(Map const* map)
            : previous_(f14CurrentMapUpdateInvocation)
        {
            f14CurrentMapUpdateInvocation = map;
        }
        F14ScopedMapUpdateInvocation(F14ScopedMapUpdateInvocation const&) = delete;
        F14ScopedMapUpdateInvocation& operator=(F14ScopedMapUpdateInvocation const&) = delete;
        ~F14ScopedMapUpdateInvocation()
        {
            f14CurrentMapUpdateInvocation = previous_;
        }

    private:
        Map const* previous_;
    };
}

bool Map::IsWithinF14MapUpdateInvocation() const
{
    return f14CurrentMapUpdateInvocation == this;
}

void Map::Update(const uint32 t_diff, const uint32 s_diff, bool  /*thread*/)
{
    F14ScopedMapUpdateInvocation f14ScopedInvocation(this);"""
    return replace_once(source, old, new, "Map.cpp scoped invocation")

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_map_update_invocation_diagnostic.py <disposable pinned Core root>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise ValueError("Core directory missing")
    before = {path: (root / path).read_text(encoding="utf-8") for path in FILES}
    if any("IsWithinF14MapUpdateInvocation" in original for original in before.values()):
        raise ValueError("patch already applied or partial mixed state; refuse")
    patched = {
        FILES[0]: edit_map_h(before[FILES[0]]),
        FILES[1]: edit_map_cpp(before[FILES[1]]),
    }
    if any(before[path] == patched[path] for path in FILES):
        raise ValueError("no-op modification; refuse")
    # Verify BOTH complete changed texts before writing EITHER file.
    for path in FILES:
        if not patched[path].strip():
            raise ValueError("empty proposed patch")
    for path in FILES:
        (root / path).write_text(patched[path], encoding="utf-8")
    print("PASS: disposable Core Map::Update frame marker applied; F14 hide remains OFF")

if __name__ == "__main__":
    main()
