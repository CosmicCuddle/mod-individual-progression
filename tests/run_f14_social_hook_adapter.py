"""Compile actual readers/scope/pair policy and social entry methods; engine doubles."""
import argparse
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument("--compiler",nargs="+",required=True);p.add_argument("--verify-parent-fails",action="store_true");a=p.parse_args()
root=Path(__file__).resolve().parents[1]
s=(subprocess.check_output(["git","show","6f029a4ef8b2dcc54adfdfccf4a0a3430017cf60:src/IndividualProgressionF14SocialIsolationScript.cpp"],cwd=root).decode("utf-8") if a.verify_parent_fails else (root/"src/IndividualProgressionF14SocialIsolationScript.cpp").read_text(encoding="utf-8"))
def extract(signature,indent):
 start=s.index(signature);end=s.index("\n"+" "*indent+"}\n",start)+indent+2;return s[start:end].replace(" override","")
readers=extract("    Policy::Human ReadHuman(",4)+extract("    Policy::Bot ReadBot(",4)
scopes=s[s.index("    struct OwnedMapRecord"):s.index("    class F14SocialIsolationPlayerScript")]
methods="\n".join(extract(sig,8) for sig in ("        bool OnPlayerCanUseChat(","        bool OnPlayerCanInitTrade(","        bool OnPlayerCanGroupInvite(","        bool OnPlayerCanGroupAccept(","        bool AllowGroupMembers(","        bool OnF14ShouldExcludeWhoEntryForViewer("))
fixture=r"""
#include "IndividualProgressionF14SocialIsolationPolicy.h"
#include <map>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
using uint32=std::uint32_t;using uint64=std::uint64_t;
namespace Policy=IndividualProgressionF14SocialIsolationPolicy;
struct ObjectGuid{uint64 id;explicit ObjectGuid(uint64 v=0):id(v){}uint64 GetRawValue()const{return id;}explicit operator bool()const{return id!=0;}bool operator==(ObjectGuid g)const{return id==g.id;}bool operator!=(ObjectGuid g)const{return id!=g.id;}};
struct WorldSession{bool headless=false;int security=0;bool IsHeadless()const{return headless;}int GetSecurity()const{return security;}};
namespace AccountMgr{bool IsPlayerAccount(int n){return n==0;}}
struct Player{uint64 id=0;std::string name;bool inWorld=true,gm=false,verified=false,poison=false;uint32 race=1;std::uint8_t stage=1;WorldSession session;
 void Touch()const{if(poison)std::abort();}WorldSession const* GetSession()const{Touch();return &session;}bool IsInWorld()const{Touch();return inWorld;}bool IsGameMaster()const{Touch();return gm;}uint32 getRace()const{Touch();return race;}ObjectGuid GetGUID()const{Touch();return ObjectGuid(id);}std::string GetName()const{Touch();return name;}};
struct IP{bool HasF14VerifiedRandomBotIdentity(Player const* p){p->Touch();return p->verified;}std::map<uint64,bool> guids;bool HasF14VerifiedRandomBotGuid(ObjectGuid g){return guids[g.id];}} ip;auto sIndividualProgression=&ip;
std::uint8_t ReadRawStage(Player const* p){p->Touch();return p->stage;}
bool enabled=true,worldCurrent=true;uint64 tick=1;int globalLookups=0,replies=0;
bool OptionOn(){return enabled;}struct Thread{bool IsCurrent(){return worldCurrent;}uint64 Tick(){return tick;}} world;auto& WorldThread(){return world;}
std::map<uint64,Player*> players;
namespace ObjectAccessor{Player* FindPlayer(ObjectGuid g){if(!worldCurrent)std::abort();++globalLookups;auto i=players.find(g.id);return i==players.end()?nullptr:i->second;}
Player* FindPlayerByName(std::string const& name,bool){if(!worldCurrent)std::abort();++globalLookups;for(auto const& v:players)if(v.second->name==name)return v.second;return nullptr;}}
struct Ref{Player* p;Player* GetSource()const{return p;}};
struct Map{std::vector<Ref> members;auto const& GetPlayers()const{return members;}};
struct Group{struct MemberSlot{ObjectGuid guid;};ObjectGuid leader{1};std::vector<MemberSlot> members;ObjectGuid GetLeaderGUID()const{return leader;}auto const& GetMemberSlots()const{return members;}};
void SendInCharacterReply(Player*,Player*){++replies;} // packet/session queue seam, not exercised by this adapter
constexpr uint32 CHAT_MSG_WHISPER=1,LANG_ADDON=0xFFFFFFFF;
"""
checks=r"""
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL check %u line %d\n",checks,__LINE__);return 1;}}while(0)
int main(){unsigned checks=0;Hook h;Player leader,human,bot,other;Map map;Group group;
 auto reset=[&](){leader=Player{};human=Player{};bot=Player{};other=Player{};leader.id=1;leader.stage=8;human.id=2;bot.id=3;bot.name="Bot";bot.race=10;bot.verified=true;bot.session.headless=true;other.id=4;
 players={{1,&leader},{2,&human},{3,&bot},{4,&other}};group.members={{ObjectGuid(1)},{ObjectGuid(2)},{ObjectGuid(3)}};map.members={{&leader},{&human},{&bot}};enabled=true;worldCurrent=true;tick=1;ThisThreadOwnedMap()={};globalLookups=0;replies=0;ip.guids.clear();};
 reset();CHECK(!h.OnPlayerCanGroupAccept(&human,&group));CHECK(replies==1);
 reset();CHECK(!h.OnPlayerCanGroupAccept(&bot,&group));CHECK(replies==0);
 reset();human.stage=8;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();bot.verified=false;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();bot.session.headless=false;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();bot.gm=true;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();bot.race=1;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();enabled=false;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();worldCurrent=false;ThisThreadOwnedMap()={&map,tick,true};CHECK(!h.OnPlayerCanGroupAccept(&bot,&group));CHECK(globalLookups==0);
 reset();worldCurrent=false;map.members={{&leader},{&bot}};human.poison=true;ThisThreadOwnedMap()={&map,tick,true};CHECK(h.OnPlayerCanGroupAccept(&bot,&group));CHECK(globalLookups==0);
 reset();worldCurrent=false;ThisThreadOwnedMap()={&map,0,true};bot.poison=true;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));CHECK(globalLookups==0);
 std::string message="hello";
 for(int direction:{0,1}){reset();Player* from=direction?&bot:&human;Player* to=direction?&human:&bot;
 CHECK(!h.OnPlayerCanUseChat(from,CHAT_MSG_WHISPER,0,message,to));CHECK(replies==(direction?0:1));
 replies=0;CHECK(!h.OnPlayerCanInitTrade(from,to));CHECK(replies==(direction?0:1));}
 reset();CHECK(h.OnPlayerCanUseChat(&human,99,0,message,&bot));CHECK(!h.OnPlayerCanUseChat(&human,CHAT_MSG_WHISPER,LANG_ADDON,message,&bot));CHECK(replies==0);
 reset();worldCurrent=false;map.members={{&human}};bot.poison=true;ThisThreadOwnedMap()={&map,tick,true};CHECK(h.OnPlayerCanInitTrade(&human,&bot));CHECK(globalLookups==0);
 reset();std::string name="Bot";CHECK(!h.OnPlayerCanGroupInvite(&human,name));
 reset();worldCurrent=false;map.members={{&human},{&bot}};ThisThreadOwnedMap()={&map,tick,true};CHECK(!h.OnPlayerCanGroupInvite(&human,name));CHECK(globalLookups==0);
 reset();worldCurrent=false;map.members={{&human}};bot.poison=true;ThisThreadOwnedMap()={&map,tick,true};CHECK(h.OnPlayerCanGroupInvite(&human,name));CHECK(globalLookups==0);
 reset();ip.guids[3]=true;CHECK(h.OnF14ShouldExcludeWhoEntryForViewer(&human,ObjectGuid(3),10));ip.guids[3]=false;CHECK(!h.OnF14ShouldExcludeWhoEntryForViewer(&human,ObjectGuid(3),10));ip.guids[3]=true;human.stage=8;CHECK(!h.OnF14ShouldExcludeWhoEntryForViewer(&human,ObjectGuid(3),10));
 reset();bot.race=11;CHECK(!h.OnPlayerCanGroupAccept(&bot,&group));
 reset();human.stage=255;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();human.session.security=1;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();bot.verified=false;bot.session.headless=false;bot.race=10;CHECK(h.OnPlayerCanInitTrade(&bot,&human));
 reset();bot.inWorld=false;CHECK(h.OnPlayerCanGroupAccept(&bot,&group));
 reset();ip.guids[3]=true;CHECK(!h.OnF14ShouldExcludeWhoEntryForViewer(&human,ObjectGuid(3),1));CHECK(!h.OnF14ShouldExcludeWhoEntryForViewer(&human,ObjectGuid(2),10));
 std::printf("%u actual social entry/reader/scope checks PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as tmp:
 r=Path(tmp);cpp=r/"social.cpp";exe=r/("social.exe" if __import__("os").name=="nt" else "social")
 cpp.write_text(fixture+readers+scopes+"struct Hook {"+methods+"};"+checks,encoding="utf-8")
 subprocess.run(a.compiler+["-std=c++17","-Wall","-Wextra","-Werror","-pthread","-I",str(root/"src"),str(cpp),"-o",str(exe)],check=True)
 result=subprocess.run([str(exe)])
 if a.verify_parent_fails:
  assert result.returncode==1,"parent must fail actual group-accept assertions"
  print("Actual parent reproduces restricted-bot non-leader-human accept bypass")
 else:result.check_returncode()
