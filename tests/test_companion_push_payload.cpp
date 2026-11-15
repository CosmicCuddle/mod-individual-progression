#include "IndividualProgressionCompanionPush.h"
#include <cassert>
#include <iostream>
using IndividualProgressionCompanionPush::Payload;
int main()
{
    unsigned checks = 0;
    for (unsigned before = 0; before <= 255; ++before)
        for (unsigned after = 0; after <= 255; ++after)
        {
            auto p = Payload(before, after);
            bool valid = before <= 18 && after <= 18 && before != after;
            assert(p.has_value() == valid);
            if (valid)
                assert(*p == "IPPSTATE\t1\t" + std::to_string(after));
            ++checks;
        }
    std::cout << checks << " raw-stage pairs PASS\n";
}
