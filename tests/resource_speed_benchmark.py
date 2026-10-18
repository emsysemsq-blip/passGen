#!/usr/bin/env python3
"""Compare automatic resource plans and durable output, alternating old/new."""
import argparse, hashlib, json, statistics, subprocess, tempfile, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',required=True);p.add_argument('--candidate',required=True);p.add_argument('--output',required=True);a=p.parse_args()
cases=[('sparse_characters',['--chars','abcd','--min','12','--max','12','--upper']),('characters',['--chars','abcd','--min','12','--max','12']),('words',['--words','a b c d','--min','12','--max','12']),('capped',['--words','a b c d','--limit','a','2','--min','12','--max','12']),('filtered_characters',['--chars','abCD12!@','--min','8','--max','8','--lower','--upper','--digit','--symbol'])]
report={'executables':{k:hashlib.sha256(Path(getattr(a,k)).read_bytes()).hexdigest() for k in ('baseline','candidate')},'scope':'Native Linux cloud CPU, automatic 70% resources, durable output; not Windows or GPU performance.','cases':[]}
with tempfile.TemporaryDirectory() as folder:
 for name,opts in cases:
  runs={'baseline':[],'candidate':[]};identity=None;settings={}
  for repeat in range(3):
   for label in (['baseline','candidate'] if repeat%2==0 else ['candidate','baseline']):
    out=Path(folder)/(name+label+str(repeat)+'.txt');start=time.perf_counter()
    result=subprocess.run([str(Path(getattr(a,label)).resolve()),*opts,'--compute','cpu','--resource-percent','70','--out',str(out)],capture_output=True,text=True,check=True)
    runs[label].append(time.perf_counter()-start);settings[label]=result.stderr.splitlines()[0]
    digest=hashlib.sha256();size=lines=0
    with out.open('rb') as stream:
     while data:=stream.read(1024*1024):digest.update(data);size+=len(data);lines+=data.count(b'\n')
    value=(size,lines,digest.hexdigest())
    if identity is None:identity=value
    assert value==identity,(name,label,value,identity)
    out.unlink();Path(str(out)+'.pgstate').unlink()
  med={k:statistics.median(v) for k,v in runs.items()};item={'case':name,'seconds':runs,'medians':med,'speedup':med['baseline']/med['candidate'],'bytes_lines_sha256':identity,'resources':settings};report['cases'].append(item);print(name,item['speedup'],flush=True)
Path(a.output).write_text(json.dumps(report,indent=2)+'\n')
