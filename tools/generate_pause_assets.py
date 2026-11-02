#!/usr/bin/env python3
"""Generate embedded RGBA pause sprites from the committed ZIP; standard library only."""
import struct,zipfile,zlib
from pathlib import Path
names=["01_logo_laura_adventure","02_faixa_pausado","03_laura_olhos_abertos","04_laura_olhos_fechados","05_coelhinho_dormindo","06_moldura_nuvens","07_botao_amarelo_superior","09_botao_roxo_superior","11_icone_continuar","12_icone_reiniciar","13_icone_musica","14_icone_casa"]
def decode(data):
 assert data[:8]==b"\x89PNG\r\n\x1a\n"
 p=8; raw=b""; width=height=mode=None
 while p<len(data):
  n=struct.unpack_from(">I",data,p)[0]; tag=data[p+4:p+8]; val=data[p+8:p+8+n];p+=n+12
  if tag==b"IHDR":
   width,height,depth,mode,_,_,interlace=struct.unpack(">IIBBBBB",val)
   assert depth==8 and mode in (2,6) and interlace==0
  elif tag==b"IDAT":raw+=val
  elif tag==b"IEND":break
 bpp=4 if mode==6 else 3; stride=width*bpp
 decoded=zlib.decompress(raw); rows=[]; prev=bytearray(stride);at=0
 for _ in range(height):
  f=decoded[at];at+=1; row=bytearray(decoded[at:at+stride]);at+=stride
  for i in range(stride):
   a=row[i-bpp] if i>=bpp else 0;b=prev[i];c=prev[i-bpp] if i>=bpp else 0
   if f==1:row[i]=(row[i]+a)&255
   elif f==2:row[i]=(row[i]+b)&255
   elif f==3:row[i]=(row[i]+((a+b)//2))&255
   elif f==4:
    q=a+b-c;da=abs(q-a);db=abs(q-b);dc=abs(q-c)
    row[i]=(row[i]+(a if da<=db and da<=dc else b if db<=dc else c))&255
   else:assert f==0
  if bpp==3:rows.extend((row[j],row[j+1],row[j+2],255) for j in range(0,stride,3))
  else:rows.extend(tuple(row[j:j+4]) for j in range(0,stride,4))
  prev=row
 return width,height,rows
out=['#pragma once','#include <cstdint>','struct PauseRun {uint32_t rgba; uint32_t count;};','struct PauseAsset {int w,h;const PauseRun* runs;int count;};']
with zipfile.ZipFile("Laura_Adventure_Assets_v2.zip") as z:
 for idx,name in enumerate(names):
  w,h,pixels=decode(z.read(name+".png"))
  runs=[]
  for rgba in pixels:
   v=(rgba[0]<<24)|(rgba[1]<<16)|(rgba[2]<<8)|rgba[3]
   if runs and runs[-1][0]==v:runs[-1]=(v,runs[-1][1]+1)
   else:runs.append((v,1))
  out.append("static const PauseRun pause_runs_%d[]={%s};"%(idx,",".join("{0x%08x,%d}"%x for x in runs)))
  out.append("static const PauseAsset pause_asset_%d={%d,%d,pause_runs_%d,%d};"%(idx,w,h,idx,len(runs)))
out.append("static const PauseAsset* const pause_assets[]={"+",".join("&pause_asset_%d"%i for i in range(len(names)))+"};")
Path("source/pause_assets_generated.h").write_text("\n".join(out)+"\n")
print("Generated",len(names),"pause sprites")
