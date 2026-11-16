import json, subprocess, sys, os, re, shutil
from concurrent.futures import ThreadPoolExecutor
S=os.environ['SCRATCH']
CORE=S+'/coresync'; NEW=S+'/txdraft/'+sys.argv[1]; OUT=sys.argv[2]; EXTRA=' '.join(sys.argv[3:])
cc=json.load(open(S+'/buildsync/compile_commands.json'))
ents=[e for e in cc if '/src/server/database/' in e['file'] and e['file'].endswith('.cpp')]
os.makedirs(OUT+'_o',exist_ok=True)
def build(e):
    cmd=re.sub(r'-include \S+cmake_pch.hxx','',e['command']).replace(CORE,NEW)+' '+EXTRA
    obj=re.search(r' -o (\S+)',cmd).group(1)
    out=os.path.abspath(OUT+'_o/'+os.path.basename(obj))
    cmd=cmd.replace(' -o '+obj,' -o '+out)
    r=subprocess.run(cmd,shell=True,cwd=e['directory'],capture_output=True,text=True)
    return e['file'],r.returncode,r.stderr[-1500:],out
with ThreadPoolExecutor(8) as ex: res=list(ex.map(build,ents))
bad=[r for r in res if r[1]]
for r in bad: print(r[0],r[2])
if bad: sys.exit(1)
if os.path.exists(OUT): os.remove(OUT)
subprocess.run(['ar','rcs',OUT]+[r[3] for r in res],check=True)
print('ok',len(res))
