// Portable tests: no Core, Worldserver or DB needed.
#include "../src/IndividualProgressionGmCommandAccessPolicy.h"
#include <cassert>

using namespace IndividualProgressionGmCommandAccessPolicy;

int main()
{
    // Players are restricted to themselves regardless of selection.
    static_assert(DecideGetTarget(true, false, false, false) == Lookup::UseSelf, "");
    static_assert(DecideGetTarget(true, false, true, true) == Lookup::UseSelf, "");
    static_assert(DecideGetTarget(true, false, true, false) == Lookup::Refuse, "");

    // GM and console continue historical target/selection resolution.
    static_assert(DecideGetTarget(true, true, false, false) == Lookup::UseOriginal, "");
    static_assert(DecideGetTarget(true, true, true, false) == Lookup::UseOriginal, "");
    static_assert(DecideGetTarget(false, false, true, false) == Lookup::UseOriginal, "");
    static_assert(DecideGetTarget(false, false, false, false) == Lookup::UseOriginal, "");

    // GM forced expansion access: warn ONLY when newly acquiring an era.
    assert(NeedsPaidGrantAudit(true, true, false, true, false, false)); // raw 7 -> 8
    assert(NeedsPaidGrantAudit(true, true, true, true, false, true));  // raw 13 -> 14
    assert(NeedsPaidGrantAudit(true, true, false, true, false, true)); // both eras
    assert(!NeedsPaidGrantAudit(true, true, true, true, true, true));  // repeated set
    assert(!NeedsPaidGrantAudit(true, true, true, false, true, false)); // revoked
    assert(!NeedsPaidGrantAudit(true, true, true, true, false, false)); // existing TBC
    assert(!NeedsPaidGrantAudit(false, true, false, true, false, true)); // feature OFF
    assert(!NeedsPaidGrantAudit(true, false, false, true, false, true)); // bot/excluded
}
