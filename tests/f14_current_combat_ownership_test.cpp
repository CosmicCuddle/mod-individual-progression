#include "IndividualProgressionF14CurrentCombatOwnership.h"
#include <cstdlib>
#include <iostream>

namespace C = IndividualProgressionF14CurrentCombatOwnership;
using T = C::Tracker<3>;

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

int main()
{
    C::ActorKey a{100,10,1000,{1,2}};
    C::ActorKey b{101,11,1001,{1,2}};
    C::CreatureKey c{200,20,{1,2}};
    C::CreatureKey c2{200,21,{1,2}};
    C::CreatureKey d{201,30,{1,2}};

    T t;
    CHECK(t.Observe(a,c) == C::ObserveResult::Recorded);
    CHECK(t.Observe(a,c) == C::ObserveResult::Unchanged);
    CHECK(t.Observe(b,c) == C::ObserveResult::Recorded);
    CHECK(t.ActiveAttackers(c) == 2);
    CHECK(t.EndActor(a));
    CHECK(t.ActiveAttackers(c) == 1);
    CHECK(t.Observe(a,d) == C::ObserveResult::Recorded);

    C::ActorKey aMap2{100,10,1002,{1,3}};
    C::CreatureKey e{202,40,{1,3}};
    CHECK(t.Observe(aMap2,e) == C::ObserveResult::Recorded);
    CHECK(t.ActiveAttackers(d) == 0);
    CHECK(t.ActiveAttackers(e) == 1);
    CHECK(t.Observe(a,d) == C::ObserveResult::Stale);

    C::ActorKey aRelog{100,12,1003,{1,2}};
    CHECK(t.Observe(aRelog,c2) == C::ObserveResult::Recorded);
    CHECK(t.Observe(aMap2,e) == C::ObserveResult::Stale);
    CHECK(t.Observe(b,c2) == C::ObserveResult::Replaced);
    CHECK(t.ActiveAttackers(c2) == 2);
    CHECK(t.RetireCreature(c) == 0);
    CHECK(t.RetireCreature(c2) == 2);
    CHECK(t.ActiveAttackers(c2) == 0);

    C::ActorKey bad{100,12,1003,{9,9}};
    CHECK(t.Observe(bad,C::CreatureKey{300,50,{9,9}}) == C::ObserveResult::Poisoned);
    CHECK(t.Inspect().poisoned);
    CHECK(t.ActiveAttackers(C::CreatureKey{300,50,{9,9}}) == 0);

    C::Tracker<1> tiny;
    CHECK(tiny.Observe(a,c) == C::ObserveResult::Recorded);
    CHECK(tiny.Observe(b,c) == C::ObserveResult::Poisoned);
    CHECK(tiny.Inspect().poisoned);
    CHECK(tiny.RetireCreature(c) == 1);

    T maps;
    CHECK(maps.Observe(a,c) == C::ObserveResult::Recorded);
    CHECK(maps.Observe(a,C::CreatureKey{c.guid,c.incarnation,{1,9}}) == C::ObserveResult::Refused);
    CHECK(maps.ActiveAttackers(c) == 1);

    std::cout << "PASS current combat ownership: multiple attackers, target/map changes, relog stale rejection, exact creature retirement and fail-visible poison\n";
    return EXIT_SUCCESS;
}