/*
 * Test ACTUAL patched Core PlayerSettings.cpp against disposable fake DB.
 * This does NOT test MySQL, Worldserver gameplay, live player data, or money.
 */
#include "Player.h"
#include <iostream>

FakeCharacterDatabase CharacterDatabase;
FakeWorld fakeWorld;
FakeWorld* sWorld = &fakeWorld;

static int checks = 0;
static void Check(bool value, char const* what)
{
    ++checks;
    if (!value)
    {
        std::fprintf(stderr, "Core settings harness failure: %s\n", what);
        std::exit(EXIT_FAILURE);
    }
}

static PreparedQueryResult RawRows(uint32 guid)
{
    auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_SETTINGS);
    statement->SetData(0, guid);
    return CharacterDatabase.Query(statement);
}

static void Save(Player& player)
{
    auto tx = std::make_shared<FakeTransaction>();
    player._SavePlayerSettings(tx);
    CharacterDatabase.Commit(tx);
}

int main()
{
    constexpr uint32 guid = 7001;
    auto const source = std::string("ip_paid_expansion");
    const std::vector<std::string> malformed{
        "x 1 0 1 1 5000 1700000000 0 0",
        "1 0 1 1 5000 -7 1700000000 0 0",
        "1 0 1 1 5000 1700000000 0 0 99999999999",
        "1 0 1 1 5000 1700000000 0 0  ",
        "1  0 1 1 5000 1700000000 0 0",
        "1 0 1 1 5000 1700000000 0 0",
        "1 0 1 1 5000 1700000000 0 0 extra",
        "01 0 1 1 5000 1700000000 0 0 ",
        "", " "
    };
    for (auto const& raw : malformed)
    {
        CharacterDatabase.rows[{guid, source}] = raw;
        Player login1(guid);
        login1._LoadCharacterSettings(RawRows(guid));
        Check(login1.m_charSettingsMap.count(source) == 0, "invalid original must not enter cache");
        // Previously a getter would implicitly create a zero settings vector.
        Check(login1.GetPlayerSetting(source, 3).value == 0, "blocked read returns safe zero");
        Check(login1.m_charSettingsMap.count(source) == 0, "blocked read cannot create paid cache");
        login1.UpdatePlayerSetting(source, 2, 1);
        Check(login1.m_charSettingsMap.count(source) == 0, "blocked direct write cannot create paid cache");
        Save(login1);
        Check(CharacterDatabase.rows[{guid, source}] == raw, "autosave must preserve original raw bytes");

        Player login2(guid);
        login2._LoadCharacterSettings(RawRows(guid));
        Check(login2.m_charSettingsMap.count(source) == 0, "second login still refuses malformed row");
        Check(login2.GetPlayerSetting(source, 3).value == 0, "second-login blocked read");
        login2.UpdatePlayerSetting(source, 0, 1);
        Save(login2);
        Check(CharacterDatabase.rows[{guid, source}] == raw, "second autosave still preserves original");

        PlayerSettingsStore::UpdateSetting(guid, source, 0, 1);
        Check(CharacterDatabase.rows[{guid, source}] == raw, "offline UpdateSetting must refuse malformed row");
    }

    auto const valid = std::string("1 0 1 1 5000 1700000000 0 0 ");
    CharacterDatabase.rows[{guid, source}] = valid;
    Player validLogin(guid);
    validLogin._LoadCharacterSettings(RawRows(guid));
    Check(validLogin.m_charSettingsMap.count(source) == 1, "valid paid row must load");
    Check(validLogin.GetPlayerSetting(source, 0).value == 1, "valid paid flag must remain intact");
    Save(validLogin);
    Check(CharacterDatabase.rows[{guid, source}] == valid, "valid paid row must not change");

    auto const grandfather = std::string("0 2 1 1 0 0 0 0 ");
    CharacterDatabase.rows[{guid, source}] = grandfather;
    Player gf(guid);
    gf._LoadCharacterSettings(RawRows(guid));
    Check(gf.GetPlayerSetting(source, 1).value == 2, "valid grandfather must remain intact");
    Save(gf);
    Check(CharacterDatabase.rows[{guid, source}] == grandfather, "grandfather saved byte-identical");

    const std::string other = "ac_default";
    const std::string unrelated = "bad 1 2";
    CharacterDatabase.rows[{guid, other}] = unrelated;
    Player otherSettings(guid);
    otherSettings._LoadCharacterSettings(RawRows(guid));
    Check(otherSettings.m_charSettingsMap.count(other) == 1, "other source parser still runs");
    Save(otherSettings);
    Check(CharacterDatabase.rows[{guid, other}] == "1 2 ", "unrelated parser behaviour unchanged");

    std::cout << "ACTUAL CORE PLAYER SETTINGS HARNESS CHECKS " << checks << " passed\n";
    return 0;
}
