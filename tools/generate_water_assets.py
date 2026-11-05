#!/usr/bin/env python3
"""Generate four embedded water textures from the uploaded PNG ZIP."""
import struct,zipfile,zlib
from pathlib import Path
def decode(data):
 assert data[:8]==b"\x89PNG\r\n\x1a\n"
 p=8;raw=b"";width=height=mode=None
 while p<len(data):
  n=struct.unpack_from(">I",data,p)[0];tag=data[p+4:p+8];val=data[p+8:p+8+n];p+=n+12
  if tag==b"IHDR":
   width,height,depth,mode,_,_,interlace=struct.unpack(">IIBBBBB",val)
   assert depth==8 and mode in (2,6) and interlace==0,(depth,mode,interlace)
  elif tag==b"IDAT":raw+=val
  elif tag==b"IEND":break
 bpp=4 if mode==6 else 3;stride=width*bpp;data=zlib.decompress(raw);prev=bytearray(stride);at=0;rows=[]
 for y in range(height):
  f=data[at];at+=1;row=bytearray(data[at:at+stride]);at+=stride
  for i in range(stride):
   a=row[i-bpp] if i>=bpp else 0;b=prev[i];c=prev[i-bpp] if i>=bpp else 0
   if f==1:row[i]=(row[i]+a)&255
   elif f==2:row[i]=(row[i]+b)&255
   elif f==3:row[i]=(row[i]+((a+b)//2))&255
   elif f==4:
    q=a+b-c;da=abs(q-a);db=abs(q-b);dc=abs(q-c)
    row[i]=(row[i]+(a if da<=db and da<=dc else b if db<=dc else c))&255
   else:assert f==0
  rows.append(bytes(row));prev=row
 return width,height,bpp,rows
out=['#pragma once','#include <cstdint>','struct WaterRun {uint32_t rgba,count;};','struct WaterAsset {int w,h;const WaterRun* runs;int count;};']
with zipfile.ZipFile('Laura_Adventure_Agua_Leve.zip') as z:
 for idx in range(4):
  name='water_frame_%02d.png'%(idx+1)
  matches=[p for p in z.namelist() if p.split('/')[-1]==name]
  if len(matches)!=1:raise RuntimeError('Missing '+name)
  w,h,bpp,rows=decode(z.read(matches[0]))
  # Reduce source 1920px strip to a manageable 960px wide embedded texture.
  step=2 if w>=1500 else 1
  runs=[]
  for row in rows:
   for x in range(0,w,step):
    i=x*bpp;r,g,b=row[i:i+3];v=(r<<24)|(g<<16)|(b<<8)|255
    if runs and runs[-1][0]==v:runs[-1]=(v,runs[-1][1]+1)
    else:runs.append((v,1))
  out.append('static const WaterRun water_runs_%d[]={%s};'%(idx,','.join('{0x%08x,%d}'%a for a in runs)))
  out.append('static const WaterAsset water_asset_%d={%d,%d,water_runs_%d,%d};'%(idx,(w+step-1)//step,h,idx,len(runs)))
out.append('static const WaterAsset* const water_assets[]={'+','.join('&water_asset_%d'%i for i in range(4))+'};')
Path('source/water_assets_generated.h').write_text('\n'.join(out)+'\n')
print('Generated 4 water frames')
