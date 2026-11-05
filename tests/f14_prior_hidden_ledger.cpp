// Pure F14 prior-suppression restoration model; NO Worldserver/client.
#include "IndividualProgressionF14PriorHiddenLedger.h"
#include <cstdio>
namespace L = IndividualProgressionF14PriorHiddenLedger;
namespace R = IndividualProgressionF14LifecycleRegistry;
static int good = 0, bad = 0;
#define CHECK(x) do { if (x) ++good; else { ++bad; std::printf("FAIL %d: %s\n", __LINE__, #x); } } while (0)
int main()
{
    L::ViewerKey vanilla{100, 10}, newLogin{100, 11}, other{200, 20};
    R::CreatureKey a{0, 0, 901}, aOtherInstance{0, 1, 901}, b{1, 0, 902};
    L::Ledger<2> l;
    CHECK(l.MayConsiderNewConcealment());
    CHECK(!l.RecordConfirmedF14Suppression(vanilla, a, 1001, false)); // generic link loss
    CHECK(!l.RecordConfirmedF14Suppression({}, a, 1001, true)); // no observer
    CHECK(!l.RecordConfirmedF14Suppression(vanilla, {}, 1001, true)); // no creature
    CHECK(!l.RecordConfirmedF14Suppression(vanilla, a, 0, true)); // no incarnation
    CHECK(l.Size() == 0);
    CHECK(l.RecordConfirmedF14Suppression(vanilla, a, 1001, true));
    CHECK(l.RecordConfirmedF14Suppression(vanilla, a, 1001, true));
    CHECK(l.Size() == 1);
    CHECK(!l.NeedsRestore(vanilla, a));
    CHECK(!l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1001, true)); // not revoked
    l.RevokeCreature(aOtherInstance); // different instance must not match
    CHECK(!l.NeedsRestore(vanilla, a));
    l.RevokeCreature(a); // stop/death/respawn even when current stamp=0
    CHECK(l.NeedsRestore(vanilla, a));
    CHECK(!l.MayConsiderNewConcealment());
    CHECK(!l.RecordConfirmedF14Suppression(other, b, 2002, true)); // no new hides
    CHECK(!l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1001, false)); // pass not ACK
    CHECK(!l.CompleteRestoreOnlyWithClientProof(newLogin, a, 1001, true)); // relog old epoch
    CHECK(!l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1002, true)); // incarnation
    CHECK(l.NeedsRestore(vanilla, a));
    CHECK(l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1001, true));
    CHECK(l.Size() == 0);
    CHECK(l.MayConsiderNewConcealment());

    CHECK(l.RecordConfirmedF14Suppression(vanilla, a, 1003, true));
    CHECK(l.RecordConfirmedF14Suppression(other, b, 2004, true));
    CHECK(!l.RecordConfirmedF14Suppression(vanilla, aOtherInstance, 1005, true)); // cap
    CHECK(l.IsPoisoned());
    CHECK(l.IsFrozen());
    CHECK(l.Size() == 2); // no evictions, old links remain tracked
    CHECK(l.PendingRestores() == 2);
    CHECK(!l.ClearFrozenAfterGlobalProof(true)); // records not restored
    CHECK(!l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1003, false));
    CHECK(l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1003, true));
    CHECK(l.CompleteRestoreOnlyWithClientProof(other, b, 2004, true));
    CHECK(l.Size() == 0);
    CHECK(!l.MayConsiderNewConcealment()); // poison sticks
    CHECK(!l.ClearFrozenAfterGlobalProof(false));
    CHECK(l.ClearFrozenAfterGlobalProof(true)); // external fully verified clear
    CHECK(l.MayConsiderNewConcealment());

    CHECK(l.RecordConfirmedF14Suppression(vanilla, a, 1007, true));
    CHECK(!l.RecordConfirmedF14Suppression(vanilla, a, 1008, true)); // recycled stamp
    CHECK(l.Size() == 1);
    CHECK(l.NeedsRestore(vanilla, a)); // old suppression not silently replaced
    CHECK(l.IsFrozen());
    CHECK(l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1007, true));
    CHECK(l.ClearFrozenAfterGlobalProof(true));
    CHECK(l.RecordConfirmedF14Suppression(vanilla, a, 1009, true));
    l.RevokeViewer(vanilla.guidRaw); // all epochs, human/Altbot never automatically hidden
    CHECK(l.NeedsRestore(vanilla, a));
    CHECK(l.CompleteRestoreOnlyWithClientProof(vanilla, a, 1009, true));

    std::printf("F14 prior-hidden pure ledger: %d/%d checks passed; NO CLIENT PROOF.\n", good, good+bad);
    return bad ? 1 : 0;
}
