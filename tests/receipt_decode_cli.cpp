// Differential-test helper: reads "<present 0|1> <hex bytes or ->" lines and
// prints the REAL IndividualProgressionReceiptPolicy::DecodeRaw status (as an
// int, enum order) for each. Used by tests/test_ip_paid_receipt_audit_tool.py.
#include "IndividualProgressionReceiptPolicy.h"

#include <iostream>
#include <string>

int main()
{
    namespace R = IndividualProgressionReceiptPolicy;
    std::string present, hex;
    while (std::cin >> present >> hex)
    {
        std::string data;
        if (hex != "-")
            for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
                data.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        R::Decoded const d = R::DecodeRaw(data, present == "1");
        std::cout << static_cast<int>(d.status) << '\n';
    }
    return 0;
}
