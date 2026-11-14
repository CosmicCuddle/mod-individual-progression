#include "IndividualProgressionF14CurrentRightsRefresh.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace R = IndividualProgressionF14CurrentRightsRefresh;

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

int main()
{
    R::Pending pending;
    CHECK(!pending.HasPending());
    CHECK(pending.Take() == 0);

    pending.Request(R::Reason::DirectDamage);
    pending.Request(R::Reason::HealOrAssist);
    CHECK(pending.HasPending());
    std::uint32_t bits = pending.Take();
    CHECK((bits & R::Bit(R::Reason::DirectDamage)) != 0);
    CHECK((bits & R::Bit(R::Reason::HealOrAssist)) != 0);
    CHECK(!pending.HasPending());

    pending.Request(R::Reason::AuraChanged);
    pending.Request(R::Reason::AuraChanged);
    CHECK(pending.Take() == R::Bit(R::Reason::AuraChanged));

    // Concurrent event producers only OR bits. No pointer or mutable Core
    // state crosses threads, and a reason cannot be lost before Take().
    std::vector<std::thread> producers;
    for (int i = 0; i < 32; ++i)
    {
        producers.emplace_back([&pending, i]()
        {
            pending.Request((i & 1) ? R::Reason::PeriodicOrSpellDamage
                                    : R::Reason::CombatStateChanged);
        });
    }
    for (auto& producer : producers)
        producer.join();
    bits = pending.Take();
    CHECK((bits & R::Bit(R::Reason::PeriodicOrSpellDamage)) != 0);
    CHECK((bits & R::Bit(R::Reason::CombatStateChanged)) != 0);

    pending.Request(R::Reason::CreatureRetired);
    pending.Clear();
    CHECK(!pending.HasPending());

    std::cout << "PASS current rights refresh pending flag: reason coalescing, one-shot consume, concurrent producer safety\n";
    return EXIT_SUCCESS;
}