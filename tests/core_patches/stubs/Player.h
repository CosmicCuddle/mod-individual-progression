#pragma once
// Disposable test stubs for compiling the ACTUAL patched Core PlayerSettings.cpp.
// Not a game server implementation, provider, or owner database.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using uint32 = std::uint32_t;
namespace ObjectGuid { using LowType = uint32; }
struct FakeGuid
{
    uint32 id;
    uint32 GetCounter() const { return id; }
};

struct PlayerSetting
{
    uint32 value = 0;
    PlayerSetting() = default;
    explicit PlayerSetting(uint32 data) : value(data) { }
};
using PlayerSettingVector = std::vector<PlayerSetting>;
using PlayerSettingMap = std::map<std::string, PlayerSettingVector>;

struct FakeField
{
    std::string data;
    template <typename T> T Get() const;
};
template <> inline std::string FakeField::Get<std::string>() const { return data; }
using Field = FakeField; // Core's public DB field alias expected by PlayerSettings.cpp

struct FakeRows
{
    std::vector<std::pair<std::string, std::string>> values;
    std::size_t next = 0;
    std::array<FakeField, 2> fields{};
    FakeField* Fetch()
    {
        if (next >= values.size())
            return nullptr;
        fields[0].data = values[next].first;
        fields[1].data = values[next].second;
        return fields.data();
    }
    bool NextRow() { return ++next < values.size(); }
};
using PreparedQueryResult = std::shared_ptr<FakeRows>;

constexpr int CHAR_SEL_CHAR_SETTINGS = 1;
constexpr int CHAR_REP_CHAR_SETTINGS = 2;
constexpr int CONFIG_PLAYER_SETTINGS_ENABLED = 3;

struct CharacterDatabasePreparedStatement
{
    int operation;
    std::array<std::string, 3> fields{};
    explicit CharacterDatabasePreparedStatement(int op) : operation(op) { }
    void SetData(std::size_t index, uint32 data) { fields[index] = std::to_string(data); }
    void SetData(std::size_t index, std::string const& data) { fields[index] = data; }
};
struct FakeTransaction
{
    std::vector<CharacterDatabasePreparedStatement*> writes;
    void Append(CharacterDatabasePreparedStatement* value) { writes.push_back(value); }
};
using CharacterDatabaseTransaction = std::shared_ptr<FakeTransaction>;

struct FakeCharacterDatabase
{
    std::map<std::pair<uint32, std::string>, std::string> rows;
    CharacterDatabasePreparedStatement* GetPreparedStatement(int op)
    {
        return new CharacterDatabasePreparedStatement(op);
    }
    PreparedQueryResult Query(CharacterDatabasePreparedStatement* stmt)
    {
        auto result = std::make_shared<FakeRows>();
        const uint32 guid = static_cast<uint32>(std::stoul(stmt->fields[0]));
        for (auto const& entry : rows)
            if (entry.first.first == guid)
                result->values.push_back({entry.first.second, entry.second});
        delete stmt;
        return result->values.empty() ? nullptr : result;
    }
    void Execute(CharacterDatabasePreparedStatement* stmt)
    {
        if (stmt->operation != CHAR_REP_CHAR_SETTINGS)
            std::abort();
        const auto guid = static_cast<uint32>(std::stoul(stmt->fields[0]));
        rows[{guid, stmt->fields[1]}] = stmt->fields[2];
        delete stmt;
    }
    void Commit(CharacterDatabaseTransaction trans)
    {
        for (auto* stmt : trans->writes)
            Execute(stmt);
        trans->writes.clear();
    }
};
extern FakeCharacterDatabase CharacterDatabase;
struct FakeWorld { bool getBoolConfig(int) const { return true; } };
extern FakeWorld* sWorld;

#define LOG_ERROR(...) do { } while (false)

namespace PlayerSettingsStore
{
    PlayerSettingVector ParseSettingsData(std::string const&);
    std::string SerializeSettingsData(PlayerSettingVector const&);
    void UpdateSetting(ObjectGuid::LowType, std::string const&, uint32, uint32);
    CharacterDatabasePreparedStatement* PrepareReplaceStatement(ObjectGuid::LowType,
        std::string const&, PlayerSettingVector const&);
}

class Player
{
public:
    explicit Player(uint32 guid) : guid_(guid) { }
    FakeGuid GetGUID() const { return {guid_}; }
    void _LoadCharacterSettings(PreparedQueryResult);
    PlayerSetting GetPlayerSetting(std::string const&, uint32);
    void _SavePlayerSettings(CharacterDatabaseTransaction);
    void UpdatePlayerSetting(std::string const&, uint32, uint32);
    PlayerSettingMap m_charSettingsMap;
private:
    uint32 guid_;
};
