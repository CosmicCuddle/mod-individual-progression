// Portable GM raid teleport model only: NO Core, DB, host or Worldserver.
#include "../src/IndividualProgressionGmTeleportPolicy.h"
#include <iostream>
using IndividualProgressionGmTeleportPolicy::CanTeleport;
int main()
{
    unsigned checks=0, failures=0;
    auto check=[&](char const* name,bool actual,bool expected) {
        ++checks;
        if(actual!=expected) {++failures;std::cerr<<"FAIL "<<name<<'\n';}
    };
    constexpr unsigned tier=13;
    check("Vanilla0",CanTeleport(60,0,tier,false,false,false,true),true);
    check("Vanilla12",CanTeleport(60,12,tier,false,false,false,true),true);
    check("Vanilla13",CanTeleport(60,13,tier,false,false,false,true),false);
    check("Vanilla18",CanTeleport(60,18,tier,false,true,false,true),false);
    check("Excluded legacy",CanTeleport(60,18,tier,true,true,false,true),true);
    check("ON no vanilla bypass",CanTeleport(60,13,tier,false,false,true,true),false);
    check("No attunement",CanTeleport(60,0,tier,false,false,false,false),false);
    check("Zero level",CanTeleport(0,0,tier,false,false,false,true),false);
    check("Zero boundary",CanTeleport(60,0,0,false,false,false,true),false);
    check("80 stage18",CanTeleport(80,18,tier,false,true,false,true),true);
    check("80 stage13 OFF",CanTeleport(80,13,tier,false,false,false,true),false);
    check("80 stage12",CanTeleport(80,12,tier,false,false,false,true),true);
    check("80 stage13 ON",CanTeleport(80,13,tier,false,false,true,true),true);
    check("80 no attune",CanTeleport(80,18,tier,true,true,true,false),false);
    check("80 excluded no automatic bypass",CanTeleport(80,18,tier,true,false,false,true),false);
    check("79 denied",CanTeleport(79,0,tier,true,true,true,true),false);
    check("71 denied",CanTeleport(71,0,tier,true,true,true,true),false);
    check("81 denied",CanTeleport(81,0,tier,true,true,true,true),false);
    std::cout<<"GM GRIM TELEPORT POLICY "<<(checks-failures)<<"/"<<checks<<" passed\n";
    return failures?1:0;
}
