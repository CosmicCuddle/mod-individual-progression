"""Actual guild remediation bodies + real policy, with Guild/Player/classifier doubles."""
import argparse
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument("--compiler",nargs="+",required=True);p.add_argument("--verify-parent-fails",action="store_true");a=p.parse_args()
root=Path(__file__).resolve().parents[1]
if a.verify_parent_fails:
 source=subprocess.check_output(["git","show","aecba064eb2e9d8d7043d454b887787407816433:src/IndividualProgressionF14SocialIsolationScript.cpp"],cwd=root).decode("utf-8")
else:source=(root/"src/IndividualProgressionF14SocialIsolationScript.cpp").read_text(encoding="utf-8")
body=source[source.index("    void ScanGuild("):source.index("    // An off-world-thread add, re-checked")]
fixture=r"""
#include "IndividualProgressionF14SocialIsolationPolicy.h"
#include <map>
#include <mutex>
#include <vector>
#include <functional>
#include <cstdio>
using uint32=std::uint32_t;using uint64=std::uint64_t;
namespace Policy=IndividualProgressionF14SocialIsolationPolicy;
struct ObjectGuid{uint64 id;explicit ObjectGuid(uint64 n=0):id(n){}uint64 GetRawValue()const{return id;}bool operator==(ObjectGuid b)const{return id==b.id;}};
struct Player{uint32 guild=1;Policy::Human human;Policy::Bot bot;uint32 GetGuildId()const{return guild;}};
std::map<uint64,Player*> online;
namespace ObjectAccessor{Player* FindPlayer(ObjectGuid g){auto i=online.find(g.id);return i==online.end()?nullptr:i->second;}}
Policy::Human ReadHuman(Player const* p){return p->human;}Policy::Bot ReadBot(Player const* p){return p->bot;}
struct Member{ObjectGuid guid;ObjectGuid GetGUID()const{return guid;}};
struct Guild{std::map<uint64,Member> members;std::vector<uint64> deleted;std::function<void()> afterDelete;
 auto const& GetUnorderedMembers()const{return members;}auto GetMemberSize()const{return members.size();}
 Member* GetMember(ObjectGuid g){auto i=members.find(g.id);return i==members.end()?nullptr:&i->second;}
 void DeleteMember(ObjectGuid g,bool a,bool b,bool c){if(a||!b||c)std::abort();members.erase(g.id);deleted.push_back(g.id);if(afterDelete)afterDelete();}};
struct Mgr{Guild* guild=nullptr;Guild* GetGuildById(uint32 id){return id==1?guild:nullptr;}} mgr;auto sGuildMgr=&mgr;
std::mutex confirmedLock;std::mutex& ConfirmedLock(){return confirmedLock;}
Policy::ConfirmedRestrictedBots confirmed;auto& Confirmed(){return confirmed;}
bool enabled=true;bool OptionOn(){return enabled;}
struct Thread{bool current=true;bool IsCurrent(){return current;}} world;auto& WorldThread(){return world;}
"""
checks=r"""
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL check %u line %d\n",checks,__LINE__);return 1;}}while(0)
int main(){unsigned checks=0;Guild g;Player h,b,c,other;
 auto reset=[&](){g=Guild{};mgr.guild=&g;online.clear();confirmed=Policy::ConfirmedRestrictedBots{};enabled=true;world.current=true;
 h=Player{};b=Player{};c=Player{};other=Player{};
 h.human={true,true,true,true,false,false,1};b.bot={true,true,true,true,false,10};c.bot=b.bot;
 for(uint64 id:{1,2,3,4})g.members.emplace(id,Member{ObjectGuid(id)});
 online={{1,&h},{2,&b},{3,&c},{4,&other}};};
 reset();RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.size()==2);CHECK(g.GetMember(ObjectGuid(1))!=nullptr);
 reset();h.human.rawStage=8;RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());
 reset();online.erase(1);RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());
 reset();h.human.verifiedRandomBot=true;RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());
 reset();h.guild=9;RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());
 reset();b.bot.verifiedRandomBotIdentity=false;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();b.bot.headlessSession=false;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();b.bot.race=1;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();b.guild=9;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();enabled=false;RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());
 reset();world.current=false;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();g.members.erase(1);RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());
 reset();g.afterDelete=[&](){c.bot.verifiedRandomBotIdentity=false;};RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.size()==1);CHECK(g.GetMember(ObjectGuid(3))!=nullptr);
 reset();g.afterDelete=[&](){h.human.rawStage=8;};RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.size()==1);
 reset();online.erase(2);confirmed.Remember(2);RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.size()==1);
 reset();online.erase(2);RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();RemoveRestrictedIfSafe(1,ObjectGuid(1),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();g.members.erase(2);RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();mgr.guild=nullptr;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 for(auto change:std::vector<std::function<void(Policy::Bot&)>>{
 [](auto& v){v.present=false;},[](auto& v){v.inWorld=false;},[](auto& v){v.gameMasterMode=true;}}){
  reset();change(b.bot);RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());}
 for(auto change:std::vector<std::function<void(Policy::Human&)>>{
 [](auto& v){v.present=false;},[](auto& v){v.inWorld=false;},[](auto& v){v.nonHeadlessSession=false;},
 [](auto& v){v.playerAccountSecurity=false;},[](auto& v){v.gameMasterMode=true;},[](auto& v){v.rawStage=255;}}){
  reset();change(h.human);RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.empty());}
 reset();confirmed.Remember(2);b.bot.verifiedRandomBotIdentity=false;RemoveRestrictedIfSafe(1,ObjectGuid(2),ObjectGuid(1));CHECK(g.deleted.empty());
 reset();g.afterDelete=[&](){enabled=false;};RemediateGuild(1,ObjectGuid(1));CHECK(g.deleted.size()==1);
 std::printf("%u actual guild-remediation checks PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as tmp:
 r=Path(tmp);cpp=r/"guild.cpp";exe=r/("guild.exe" if __import__("os").name=="nt" else "guild")
 cpp.write_text(fixture+body+checks,encoding="utf-8")
 subprocess.run(a.compiler+["-std=c++17","-Wall","-Wextra","-Werror","-pthread","-I",str(root/"src"),str(cpp),"-o",str(exe)],check=True)
 result=subprocess.run([str(exe)])
 if a.verify_parent_fails:
  assert result.returncode==1,"parent must reproduce executable assertion failure"
  print("Actual parent reproduces stale-human guild-removal regression")
 else:result.check_returncode()
