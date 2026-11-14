#include "IndividualProgressionF14CurrentViewerSession.h"
#include <cstdlib>
#include <iostream>
using namespace IndividualProgressionF14CurrentViewerSession;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while(false)
int main()
{
    State s;
    MapKey a{0,0}, b{1,42};
    CHECK(!s.Enter(10,a,1) && !s.Read(10,a));
    s.BindLogin(10,7);
    CHECK(!s.Read(10,a));
    CHECK(s.Enter(10,a,1));
    auto old = s.Read(10,a);
    CHECK(old && old.epoch == 7 && old.generation == 1);
    CHECK(!s.Read(11,a) && !s.Read(10,b));
    s.Leave(10,a);
    CHECK(!s.Read(10,a));
    CHECK(s.Enter(10,b,2));
    CHECK(s.Read(10,b).generation == 2 && !s.Read(10,a));
    s.Leave(10,a); // stale wrong-map callback cannot leave new authority live
    CHECK(!s.Read(10,b) && !s.Enter(10,a,3));
    s.Logout();
    CHECK(!s.Read(10,b) && !s.Enter(10,a,4));
    s.BindLogin(10,8);
    CHECK(s.Enter(10,a,5) && s.Read(10,a).epoch == 8);
    s.Leave(10,a);
    CHECK(!s.Enter(10,a,5)); // reused generation
    s.BindLogin(10,9);
    CHECK(s.Enter(10,a,6));
    CHECK(!s.Enter(10,a,7) && !s.Read(10,a)); // duplicate/missed leave
    s.BindLogin(10,10);
    CHECK(!s.Enter(10,a,0));
    s.BindLogin(10,11);
    CHECK(s.Enter(10,a,8));
    s.InvalidateMap();
    CHECK(!s.Read(10,a) && !s.Enter(10,b,9));
    s.BindLogin(0,12);
    CHECK(!s.Enter(10,a,10));
    std::cout << "PASS current viewer login, map handoff, logout and stale generation\n";
}
