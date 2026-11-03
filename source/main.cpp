#include <switch.h>
#include <SDL.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <array>
#include <cstring>
#include "laura_voice.h"
#include "laura_frames.h"
#include "level_sounds.h"
#include "level_complete.h"
#include "lauras_theme.h"
#include "lauras_sunset.h"
#include "lauras_cloud_hop.h"
#include "title_art.h"
#include "pause_assets_generated.h"
#include "complete_assets_generated.h"
#include "terrain_assets_generated.h"

constexpr int SW=1280, SH=720;
constexpr float WORLD=5650.0f, GROUND=615.0f;
struct Voice {const int16_t* data=nullptr; int len=0,pos=0; float volume=1;};
struct Mixer { Voice voices[8]; bool musicPlaying=true; int musicTrack=0; int musicPos=0; float musicVolume=0.25f; };
static void audioCallback(void* p, Uint8* stream,int len) {
 Mixer& m=*static_cast<Mixer*>(p); auto* dst=(int16_t*)stream;
 for(int i=0;i<len/2;++i){int sum=0;
  for(auto& v:m.voices) if(v.data && v.pos<v.len){sum+=int(v.data[v.pos++]*v.volume);if(v.pos==v.len)v.data=nullptr;}
  // All three original soundtracks are 24 kHz mono; output is 48 kHz mono.
  if(m.musicPlaying){
   // 0: titulo; 1: fase 1; 2: fase 2 (Laura's Cloud Hop).
   const int8_t* track=m.musicTrack==0?lauras_sunset_pcm:
      (m.musicTrack==2?lauras_cloud_hop_pcm:lauras_theme_pcm);
   const int count=m.musicTrack==0?lauras_sunset_count:
      (m.musicTrack==2?lauras_cloud_hop_count:lauras_theme_count);
   if(count>1){
    int idx=(m.musicPos/2)%count;
    int next=(idx+1<count)?idx+1:(m.musicTrack==2?lauras_cloud_hop_overlap:0);
    int value=(m.musicPos&1)?(int(track[idx])+int(track[next]))/2:int(track[idx]);
    sum+=int(value*256*m.musicVolume);
    ++m.musicPos;
    if(m.musicPos>=count*2){
     // A ultima janela de 180 ms da Fase 2 e cruzada com a abertura.
     // Depois da primeira volta, pular essa abertura evita estalo no loop.
     m.musicPos=(m.musicTrack==2)?(lauras_cloud_hop_overlap*2):0;
    }
   }
  }
  dst[i]=(int16_t)std::max(-32768,std::min(32767,sum));
 }
}
static void play(SDL_AudioDeviceID dev,Mixer& m,const int16_t* data,int count,float vol=0.7f){
 if(!dev||!data||count<=0)return;
 SDL_LockAudioDevice(dev);
 Voice* slot=nullptr; for(auto& v:m.voices)if(!v.data){slot=&v;break;}
 if(!slot)slot=&m.voices[0]; *slot={data,count,0,vol};
 SDL_UnlockAudioDevice(dev);
}
struct Rect {float x,y,w,h;};
static bool overlap(const Rect& a,const Rect& b){return a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y;}
static void fill(SDL_Renderer*r,float x,float y,float w,float h,Uint8 R,Uint8 G,Uint8 B,int cam=0){
 SDL_SetRenderDrawColor(r,R,G,B,255); SDL_Rect a{int(x-cam),int(y),int(w),int(h)};SDL_RenderFillRect(r,&a);
}
static void line(SDL_Renderer*r,int x,int y,int x2,int y2,Uint8 R,Uint8 G,Uint8 B,int cam=0){SDL_SetRenderDrawColor(r,R,G,B,255);SDL_RenderDrawLine(r,x-cam,y,x2-cam,y2);}

// Stage 1 textures are drawn over the original geometric surfaces; physics is unchanged.
static SDL_Texture* terrainTextures[9]{};
static void initTerrainTextures(SDL_Renderer*r){
 for(int i=0;i<9;i++){
  const TerrainAsset&a=*terrain_assets[i];
  SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,a.w,a.h,32,SDL_PIXELFORMAT_RGBA8888);
  if(!s)continue;
  Uint32*px=(Uint32*)s->pixels;int pos=0;
  for(int j=0;j<a.count;j++){
   const TerrainRun&run=a.runs[j];
   Uint32 color=SDL_MapRGBA(s->format,Uint8(run.rgba>>24),Uint8(run.rgba>>16),Uint8(run.rgba>>8),Uint8(run.rgba));
   for(uint32_t k=0;k<run.count;k++)px[pos++]=color;
  }
  terrainTextures[i]=SDL_CreateTextureFromSurface(r,s);SDL_FreeSurface(s);
  if(terrainTextures[i])SDL_SetTextureBlendMode(terrainTextures[i],SDL_BLENDMODE_NONE);
 }
}
static void textureFill(SDL_Renderer*r,int id,float x,float y,float w,float h,int cam){
 if(id<0||id>=9||!terrainTextures[id])return;
 SDL_Texture*t=terrainTextures[id];int tw=0,th=0;
 SDL_QueryTexture(t,nullptr,nullptr,&tw,&th);if(tw<=0||th<=0)return;
 int left=int(x)-cam,top=int(y),right=int(x+w)-cam,bottom=int(y+h);
 // Cull offscreen tiles; world-anchored pattern prevents visible swimming.
 for(int yy=top;yy<bottom;){
  int sy=((yy%th)+th)%th,dy=std::min(th-sy,bottom-yy);
  for(int xx=left;xx<right;){
   int sx=(((xx+cam)%tw)+tw)%tw,dx=std::min(tw-sx,right-xx);
   if(xx+dx>0&&xx<SW&&yy+dy>0&&yy<SH){
    SDL_Rect src{sx,sy,dx,dy},dst{xx,yy,dx,dy};
    SDL_RenderCopy(r,t,&src,&dst);
   }
   xx+=dx;
  }
  yy+=dy;
 }
}

static const char* glyph(char c){switch(c){
 case 'A':return "011101000110001111111000110001";case 'B':return "11110100011000111110100011000111110";
 case 'C':return "01111100001000010000100001000001111";case 'D':return "11110100011000110001100011000111110";
 case 'E':return "11111100001000011110100001000011111";case 'F':return "11111100001000011110100001000010000";
 case 'G':return "01111100001000010111100011000101111";case 'H':return "10001100011000111111100011000110001";
 case 'I':return "11111001000010000100001000010011111";case 'J':return "00111000100001000010100101001001100";
 case 'K':return "10001100101010011000101001001010001";case 'L':return "10000100001000010000100001000011111";
 case 'M':return "10001110111010110101100011000110001";case 'N':return "10001110011010110011100011000110001";
 case 'O':return "01110100011000110001100011000101110";case 'P':return "11110100011000111110100001000010000";
 case 'Q':return "01110100011000110001101011001001101";case 'R':return "11110100011000111110101001001010001";
 case 'S':return "01111100001000001110000010000111110";case 'T':return "11111001000010000100001000010000100";
 case 'U':return "10001100011000110001100011000101110";case 'V':return "10001100011000110001100010101000100";
 case 'X':return "10001100010101000100010101000110001";case 'Y':return "10001100010101000100001000010000100";
 case 'Z':return "11111000010001000100010001000011111";case '-':return "00000000000000011111000000000000000";
 case '1':return "00100011000010000100001000010001110";case '0':return "01110100011001110101110011000101110";
 case ' ':return "00000000000000000000000000000000000";default:return "00000000000000000000000000000000000";
 }}
static void label(SDL_Renderer*r,const char* s,int x,int y,int scale,Uint8 R,Uint8 G,Uint8 B,int cam){
 for(;*s;s++,x+=6*scale){const char* g=glyph(*s);for(int k=0;k<35&&g[k];k++)if(g[k]=='1')fill(r,x+(k%5)*scale,y+(k/5)*scale,scale,scale,R,G,B,cam);}
}
static void building(SDL_Renderer*r,int x,int top,int w,int h,Uint8 cr,Uint8 cg,Uint8 cb,int cam,bool windows=true){
 fill(r,x,top,w,h,cr,cg,cb,cam);textureFill(r,cr<120?0:cr>200?1:cr>185?2:3,x,top,w,h,cam);fill(r,x,top,w,9,98,66,52,cam);
 if(windows)for(int i=0;i<w/80;i++){int wx=x+25+i*80;fill(r,wx,top+45,39,52,65,79,84,cam);fill(r,wx+4,top+49,31,44,122,166,176,cam);fill(r,wx+18,top+49,3,44,54,63,66,cam);}
 for(int i=0;i<w/45;i++)fill(r,x+17+i*45,top+h-32,21,3,cr-std::min<int>(cr,20),cg-std::min<int>(cg,20),cb-std::min<int>(cb,20),cam);
}
static void crate(SDL_Renderer*r,float x,float y,int cam){fill(r,x,y,64,64,105,64,33,cam);fill(r,x+5,y+5,54,54,167,113,61,cam);line(r,x+7,y+7,x+57,y+57,91,57,34,cam);line(r,x+57,y+7,x+7,y+57,91,57,34,cam);}
static void rabbit(SDL_Renderer*r,float x,float y,int cam){
 fill(r,x+13,y+9,7,24,250,242,241,cam);fill(r,x+27,y+6,7,27,250,242,241,cam);
 fill(r,x+15,y+12,3,17,246,157,188,cam);fill(r,x+29,y+9,3,18,246,157,188,cam);
 fill(r,x+8,y+28,32,29,245,241,235,cam);fill(r,x+13,y+55,26,8,242,237,233,cam);
 fill(r,x+29,y+39,4,4,35,36,44,cam);fill(r,x+38,y+47,5,4,239,131,163,cam);
}
static void scene(SDL_Renderer*r,float cam,float time,const std::vector<Rect>& blocks,const std::array<Rect,3>& moving,const std::vector<Rect>& returnPlatforms,bool collected){
 fill(r,0,0,SW,SH,127,195,231);
 // Parallax distante: morros e predios (0.14x)
 for(int i=0;i<22;i++){int px=i*210-int(cam*.14f);fill(r,px,235+(i%4)*16,190,GROUND-(235+(i%4)*16),94,157,156);fill(r,px+65,170+(i%3)*35,75,GROUND-(170+(i%3)*35),171,180,170);}
 // Predios de condominio (0.25x)
 for(int i=0;i<23;i++){int px=i*265-int(cam*.25f);int top=250+(i%4)*21;fill(r,px,top,110,GROUND-top,196,184,161);for(int j=0;j<3;j++)for(int k=0;k<4;k++)fill(r,px+14+j*32,top+30+k*54,15,24,99,125,138);}
 // Casas e arvores no plano medio (0.43x)
 for(int i=0;i<34;i++){int px=i*175-int(cam*.43f);fill(r,px,400+(i%3)*14,118,GROUND-(400+(i%3)*14),204,167,134);fill(r,px-8,389+(i%3)*14,132,17,152,79,62);// Background roofs use their original solid shading to avoid texture swimming during parallax.
  fill(r,px+137,390,9,GROUND-390,93,67,46);fill(r,px+103,344,76,55,58,122,74);fill(r,px+114,327,53,45,72,137,80);}
 // Vila
 building(r,0,312,460,303,88,144,143,int(cam),false);building(r,460,310,585,305,206,164,111,int(cam),false);
 fill(r,12,362,430,40,36,102,111,int(cam));label(r,"MERCADO",92,371,4,245,213,151,int(cam));
 fill(r,470,358,563,41,180,99,67,int(cam));label(r,"PADARIA",620,366,4,255,222,166,int(cam));
 // Vitrines, mercadorias e pessoas (figuras simples)
 fill(r,60,428,335,145,49,72,78,int(cam));for(int i=0;i<4;i++){fill(r,68,446+i*29,320,4,140,100,61,int(cam));for(int j=0;j<9;j++)fill(r,77+j*34,429+i*29,19,16,194+(i%2)*30,136,62,int(cam));}
 for(int i=0;i<2;i++){int px=270+i*82;fill(r,px,474,21,26,194,135,105,int(cam));fill(r,px-7,499,35,47,66,93,120,int(cam));}
 fill(r,535,450,250,116,129,190,195,int(cam));fill(r,545,460,230,98,193,221,219,int(cam));fill(r,540,525,240,12,115,73,49,int(cam));for(int i=0;i<8;i++)fill(r,552+i*27,489+(i%2)*13,22,18,206,145,71,int(cam));
 fill(r,815,433,94,145,71,101,109,int(cam));for(int i=0;i<3;i++)fill(r,825,448+i*37,75,27,190,218,220,int(cam));
 // telhas da padaria e mercado
 for(int i=0;i<26;i++){fill(r,i*42,299+(i%2)*3,40,14,144+(i%3)*12,68,51,int(cam));textureFill(r,4,i*42,299+(i%2)*3,40,14,int(cam));}
 // Rua e muros de casas
 building(r,1270,277,320,338,180,157,133,int(cam));building(r,1680,335,430,280,191,166,137,int(cam));
 fill(r,1490,427,590,188,116,110,108,int(cam));textureFill(r,5,1490,427,590,188,int(cam));for(int i=0;i<16;i++)fill(r,1495+i*36,438+(i%3)*11,24,3,117,108,96,int(cam));
 // Muro de separacao: chega ate o solo e bloqueia acesso lateral ao corrego.
 fill(r,2110,430,210,185,111,108,110,int(cam));textureFill(r,5,2110,430,210,185,int(cam));
 fill(r,2110,425,210,12,187,174,147,int(cam));
 for(int i=0;i<5;i++)fill(r,2120+i*40,470+(i%2)*24,26,4,111,105,91,int(cam));
 // grade: vazada, NAO e solida
 for(int i=0;i<9;i++)fill(r,1445+i*16,435,5,180,63,69,68,int(cam));
 fill(r,1440,435,152,9,55,61,59,int(cam));
 // Calçadas e beiradas dos corrego
 auto ground=[&](float a,float b,float top){fill(r,a,top,b-a,SH-top,86,85,76,int(cam));fill(r,a,top,b-a,13,171,167,147,int(cam));textureFill(r,6,a,top,b-a,13,int(cam));for(int i=int(a);i<b;i+=54)line(r,i,int(top+13),i+12,int(top+13),61,64,62,int(cam));};
 ground(0,2320,615);ground(2660,3170,615);ground(4510,5650,615);
 // agua dos dois corregos
 for(int n=0;n<2;n++){int a=n==0?2320:3170,b=n==0?2660:4510;
  fill(r,a,650,b-a,SH-650,24,95,116,int(cam));
  // Exposed canal walls and dark gray coping below the pavement.
  fill(r,a,615,14,35,70,68,73,int(cam));fill(r,b-14,615,14,35,70,68,73,int(cam));
  for(int yy=619;yy<648;yy+=11){fill(r,a,yy,14,2,112,108,110,int(cam));fill(r,b-14,yy,14,2,112,108,110,int(cam));}
  fill(r,a,650,b-a,5,67,176,191,int(cam));
  for(int yy=664;yy<SH;yy+=22)for(int xx=a+12;xx<b-12;xx+=58){
   int phase=int(4*std::sin(time*1.8f+xx*.045f+yy*.03f));
   fill(r,xx+phase,yy,25,3,35,125,150,int(cam));
   fill(r,xx+phase+8,yy+5,13,2,111,201,211,int(cam));
  }
  for(int xx=a+8;xx<b-20;xx+=43){int wave=int(3*std::sin(time*2.2f+xx*.065f));fill(r,xx,652+wave,23,2,164,224,222,int(cam));}
 }
 // Crocodile: shaded pixel-art silhouette with animated tail, scales and eyes.
 float crocX=3730+145*std::sin(time*.72f), bob=3*std::sin(time*2.8f);
 float tail=22*std::sin(time*3.1f);
 // Tail layers and segmented dorsal ridge.
 fill(r,crocX-105,684+tail*.48f,43,7,30,69,48,int(cam));
 fill(r,crocX-78,679+tail*.30f,87,15,38,89,55,int(cam));
 fill(r,crocX-60,682+tail*.22f,80,7,83,146,69,int(cam));
 fill(r,crocX,665+bob,178,33,29,77,46,int(cam));
 fill(r,crocX+4,669+bob,169,22,61,125,60,int(cam));
 fill(r,crocX+11,688+bob,158,7,104,139,73,int(cam));
 for(int j=0;j<10;j++){
  float xx=crocX+5+j*17;
  fill(r,xx,659+bob+(j%2)*3,13,8,29,69,42,int(cam));
  fill(r,xx+3,661+bob+(j%2)*3,7,3,105,160,72,int(cam));
 }
 for(int j=0;j<8;j++){
  fill(r,crocX+12+j*20,676+bob+(j%2)*5,10,5,34,95,52,int(cam));
  fill(r,crocX+17+j*20,681+bob+(j%2)*5,5,3,131,173,82,int(cam));
 }
 for(int j=0;j<2;j++){
  float xx=crocX+22+j*111;
  fill(r,xx,692+bob,26,11,30,68,41,int(cam));
  fill(r,xx+4,699+bob,22,4,70,113,55,int(cam));
 }
 // Distinct head, snout, jaw, nostril and blink.
 fill(r,crocX+158,653+bob,52,37,31,82,47,int(cam));
 fill(r,crocX+164,658+bob,45,28,77,139,66,int(cam));
 fill(r,crocX+194,670+bob,62,19,43,103,53,int(cam));
 fill(r,crocX+204,672+bob,50,9,102,155,72,int(cam));
 fill(r,crocX+196,686+bob,58,7,28,63,44,int(cam));
 for(int j=0;j<6;j++)fill(r,crocX+201+j*9,683+bob,4,5,242,229,177,int(cam));
 fill(r,crocX+181,658+bob,16,11,29,65,40,int(cam));
 bool blink=std::fmod(time,5.2f)<.13f;
 fill(r,crocX+185,661+bob,10,blink?2:7,250,222,111,int(cam));
 if(!blink)fill(r,crocX+190,662+bob,4,5,22,31,26,int(cam));
 fill(r,crocX+241,673+bob,5,4,23,49,34,int(cam));
 // Escadaria solida visualmente: degraus apoiados no terreno e colisoes no topo.
 for(int i=0;i<8;i++){float xx=4510+i*76, yy=615-i*35;fill(r,xx,yy,80,SH-yy,96,97,88,int(cam));fill(r,xx,yy,80,9,178,176,150,int(cam));}
 // A escola tem alicerce ate o solo, sem vazio embaixo.
 fill(r,5050,370,600,SH-370,96,97,88,int(cam));
 fill(r,5050,370,600,10,178,176,150,int(cam));
 building(r,5050,165,600,205,113,167,113,int(cam),false);fill(r,5060,175,575,40,79,132,86,int(cam));
 // Windows below the rainbow banner but above the school name, never behind signs.
 for(int i=0;i<6;i++){int wx=5074+i*92;fill(r,wx,226,43,27,55,91,88,int(cam));fill(r,wx+4,229,35,20,150,206,209,int(cam));fill(r,wx+20,229,3,20,65,96,98,int(cam));}
 // arco iris simplificado em faixas
 for(int i=0;i<5;i++){Uint8 cols[5][3]={{220,86,80},{230,161,70},{220,208,84},{95,175,111},{96,137,204}};fill(r,5200+i*11,190+i*8,275-i*22,8,cols[i][0],cols[i][1],cols[i][2],int(cam));}
 label(r,"ESCOLA ARCO-IRIS",5120,264,3,239,244,220,int(cam));fill(r,5270,286,112,84,35,80,69,int(cam));fill(r,5321,286,5,84,181,204,187,int(cam));
 // Caixas e superficie do muro; telhados usam a propria arte das construcoes.
 for(const auto& b:blocks){
  // Dark graphite coping on top of solid masonry walls, not terracotta roof red.
  if(b.w>90&&b.h>30&&b.y<615){
   fill(r,b.x,b.y-3,b.w,8,69,70,78,int(cam));
   for(int tx=int(b.x)+8;tx<int(b.x+b.w)-4;tx+=36)
    fill(r,tx,b.y-2,19,2,117,120,127,int(cam));
  }
  if(b.w==64&&b.h==64)crate(r,b.x,b.y,int(cam));
 }
 for(const auto& p:returnPlatforms){fill(r,p.x,p.y,p.w,p.h,111,132,145,int(cam));fill(r,p.x,p.y,p.w,6,176,207,207,int(cam));fill(r,p.x+9,p.y+10,p.w-18,4,64,93,109,int(cam));}
 for(auto& p:moving){fill(r,p.x,p.y,p.w,p.h,82,125,152,int(cam));fill(r,p.x,p.y,p.w,6,139,200,213,int(cam));fill(r,p.x+8,p.y+10,p.w-16,5,48,82,98,int(cam));}
 if(!collected)rabbit(r,780,240,int(cam));
}
#include "level2.h"
// Pause menu shared by both stages; warm colors echo the title screen.
static SDL_Texture* pauseTextures[12]{};
static void initPauseTextures(SDL_Renderer* r){
 for(int i=0;i<12;i++){
  const PauseAsset& a=*pause_assets[i];
  SDL_Surface* surf=SDL_CreateRGBSurfaceWithFormat(0,a.w,a.h,32,SDL_PIXELFORMAT_RGBA8888);
  if(!surf)continue;
  Uint32* pixels=(Uint32*)surf->pixels;size_t pos=0;
  for(int j=0;j<a.count;j++){
   const PauseRun& run=a.runs[j];
   Uint32 c=SDL_MapRGBA(surf->format,Uint8(run.rgba>>24),Uint8(run.rgba>>16),Uint8(run.rgba>>8),Uint8(run.rgba));
   for(uint32_t k=0;k<run.count;k++)pixels[pos++]=c;
  }
  pauseTextures[i]=SDL_CreateTextureFromSurface(r,surf);SDL_FreeSurface(surf);
  if(pauseTextures[i])SDL_SetTextureBlendMode(pauseTextures[i],SDL_BLENDMODE_BLEND);
 }
}
static void pauseSprite(SDL_Renderer* r,int id,int x,int y,int w,int h){
 if(id<0||id>=12||!pauseTextures[id])return;
 SDL_Rect dst{x,y,w,h};SDL_RenderCopy(r,pauseTextures[id],nullptr,&dst);
}
static void pauseMenu(SDL_Renderer* r,int selection,bool bgmEnabled){
 // The gameplay remains visible beneath a translucent rose veil.
 SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
 SDL_SetRenderDrawColor(r,222,112,172,145);
 SDL_Rect screen{0,0,SW,SH};SDL_RenderFillRect(r,&screen);
 SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
 // Layered artwork: border, title, characters and independent interactive buttons.
 pauseSprite(r,5,260,54,760,610);
 pauseSprite(r,0,390,76,500,135);
 pauseSprite(r,1,492,195,296,86);
 pauseSprite(r,2,178,385,250,198);
 pauseSprite(r,4,888,429,207,178);
 for(int i=0;i<4;i++){
  int y=280+i*72;
  pauseSprite(r,selection==i?6:7,422,y,436,67);
  pauseSprite(r,8+i,447,y+12,44,42);
  const char* name=i==0?"CONTINUAR":i==1?"REINICIAR FASE":i==2?
   (bgmEnabled?"BGM: LIGADA":"BGM: DESLIGADA"):"MENU PRINCIPAL";
  const int scale=3;int width=int(std::strlen(name))*6*scale;
  label(r,name,650-width/2,y+21,scale,selection==i?77:255,selection==i?49:245,selection==i?94:233,0);
 }
 // Compact hints, safely within the panel.
 label(r,"D-PAD: MOVER",428,598,2,255,246,225,0);
 label(r,"A: OK",604,598,2,255,246,225,0);
 label(r,"B/+: VOLTAR",703,598,2,255,246,225,0);
}

static SDL_Texture* completeTextures[13]{};
static void initCompleteTextures(SDL_Renderer* r){
 for(int i=0;i<13;i++){
  const CompleteAsset& a=*complete_assets[i];
  SDL_Surface* surf=SDL_CreateRGBSurfaceWithFormat(0,a.w,a.h,32,SDL_PIXELFORMAT_RGBA8888);
  if(!surf)continue;
  Uint32* pixels=(Uint32*)surf->pixels;size_t pos=0;
  for(int j=0;j<a.count;j++){
   const CompleteRun& run=a.runs[j];
   Uint32 c=SDL_MapRGBA(surf->format,Uint8(run.rgba>>24),Uint8(run.rgba>>16),Uint8(run.rgba>>8),Uint8(run.rgba));
   for(uint32_t k=0;k<run.count;k++)pixels[pos++]=c;
  }
  completeTextures[i]=SDL_CreateTextureFromSurface(r,surf);SDL_FreeSurface(surf);
  if(completeTextures[i])SDL_SetTextureBlendMode(completeTextures[i],SDL_BLENDMODE_BLEND);
 }
}
static void completeSprite(SDL_Renderer*r,int i,int x,int y,int w,int h){
 if(i<0||i>=13||!completeTextures[i])return;
 SDL_Rect dst{x,y,w,h};SDL_RenderCopy(r,completeTextures[i],nullptr,&dst);
}
static void completeMenu(SDL_Renderer*r,int stage,int choice){
 SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
 SDL_SetRenderDrawColor(r,224,114,185,138);
 SDL_Rect screen{0,0,SW,SH};SDL_RenderFillRect(r,&screen);
 SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
 completeSprite(r,8,258,47,764,622);
 completeSprite(r,0,421,61,438,123);
 completeSprite(r,1,368,180,544,95);
 completeSprite(r,12,389,277,503,49);
 const char* msg=stage==1?"LAURA CHEGOU A ESCOLA":"LAURA ENCONTROU A MAMAE";
 int scale=3;int width=int(std::strlen(msg))*6*scale;
 label(r,msg,640-width/2,291,scale,75,34,119,0);
 for(int i=0;i<2;i++){
  int y=357+i*87;
  completeSprite(r,(i==0?(choice==0?2:3):(choice==1?4:5)),403,y,474,81);
  const char* txt=i==0?"REINICIAR":"PROXIMO NIVEL";
  int tw=int(std::strlen(txt))*6*3;
  label(r,txt,660-tw/2,y+26,3,choice==i?75:255,choice==i?35:250,choice==i?90:245,0);
 }
 completeSprite(r,6,178,421,249,227);
 completeSprite(r,7,843,444,224,169);
 label(r,"D-PAD: ESCOLHER   A: CONFIRMAR",397,614,2,255,243,224,0);
}

int main(int,char**){
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)!=0)return 1;
 SDL_Window* win=SDL_CreateWindow("Laura Adventure 1.2.1 - Fase 2",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,SW,SH,SDL_WINDOW_SHOWN);
 if(!win){SDL_Quit();return 2;}
 SDL_Renderer* r=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
 if(!r)r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);
 if(!r){SDL_DestroyWindow(win);SDL_Quit();return 3;}
 initPauseTextures(r);
 initCompleteTextures(r);
 initTerrainTextures(r);
 auto loadFrame=[&](const FrameData& frame)->SDL_Texture*{
  SDL_Surface* s=SDL_CreateRGBSurfaceWithFormat(0,frame.w,frame.h,32,SDL_PIXELFORMAT_RGBA8888);if(!s)return nullptr;
  SDL_LockSurface(s);Uint32* px=(Uint32*)s->pixels;int pos=0;
  for(int i=0;i<frame.runCount;i++){const SpriteRun& run=frame.runs[i];Uint32 color=SDL_MapRGBA(s->format,Uint8(run.rgba>>24),Uint8(run.rgba>>16),Uint8(run.rgba>>8),Uint8(run.rgba));for(int j=0;j<run.count;j++)px[pos++]=color;}
  SDL_UnlockSurface(s);SDL_Texture*t=SDL_CreateTextureFromSurface(r,s);SDL_FreeSurface(s);if(t)SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);return t;
 };
 const FrameData* frames[]={&frame_parado_001,&frame_corrida_001,&frame_corrida_002,&frame_corrida_003,&frame_corrida_004,&frame_corrida_005,&frame_corrida_006,&frame_salto_001,&frame_salto_002,&frame_salto_003,&frame_salto_004,&frame_aterrissagem_001,&frame_aterrissagem_002,&frame_aterrissagem_003,&frame_aterrissagem_004,&frame_dano_001,&frame_dano_002,&frame_dano_003};
 SDL_Texture* anim[18]{};for(int i=0;i<18;i++){anim[i]=loadFrame(*frames[i]);if(!anim[i]){SDL_Quit();return 4;}}
 // Carregar somente os 6 PNGs integrados em mamae_frames.h (sem arquivos externos).
 auto loadMamae=[&](const MamaeFrame& frame)->SDL_Texture*{
  SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormat(0,frame.w,frame.h,32,SDL_PIXELFORMAT_RGBA8888);
  if(!surface)return nullptr;
  SDL_LockSurface(surface);
  Uint32* pixels=(Uint32*)surface->pixels;int pos=0;
  for(int k=0;k<frame.runCount;k++){
   const MamaeRun& run=frame.runs[k];
   Uint32 color=SDL_MapRGBA(surface->format,Uint8(run.rgba>>24),Uint8(run.rgba>>16),Uint8(run.rgba>>8),Uint8(run.rgba));
   for(uint32_t j=0;j<run.count;j++)pixels[pos++]=color;
  }
  SDL_UnlockSurface(surface);
  SDL_Texture* t=SDL_CreateTextureFromSurface(r,surface);SDL_FreeSurface(surface);
  if(t)SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
  return t;
 };
 const MamaeFrame* momFrames[]={&mamae_esperando_001,&mamae_esperando_002,&mamae_interacao_001,&mamae_interacao_002,&mamae_balao_exclamacao,&mamae_balao_coracao};
 SDL_Texture* mom[6]{};for(int i=0;i<6;i++)mom[i]=loadMamae(*momFrames[i]);
 Level2 level2;
 bool stage2=false;
 SDL_Texture* titleTexture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STATIC,title_art_width,title_art_height);
 if(titleTexture){
  SDL_UpdateTexture(titleTexture,nullptr,title_art_rgb565,title_art_width*int(sizeof(uint16_t)));
  SDL_SetTextureBlendMode(titleTexture,SDL_BLENDMODE_NONE);
 }
 Mixer mix{};SDL_AudioSpec want{};want.freq=48000;want.format=AUDIO_S16SYS;want.channels=1;want.samples=1024;want.callback=audioCallback;want.userdata=&mix;
 SDL_AudioDeviceID audio=SDL_OpenAudioDevice(nullptr,0,&want,nullptr,0);if(audio){mix.musicPlaying=true;mix.musicTrack=0;mix.musicPos=0;SDL_PauseAudioDevice(audio,0);}
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
 // Superficies de mao unica: atravessaveis por baixo, pisaveis por cima.
 // O muro e fisicamente intransponivel pelas laterais.
 std::vector<Rect> oneWay={
  {0,295,460,16}, {460,295,585,16}, // comercio
  {1270,277,320,12}, {1680,335,430,12}, // telhados das casas
  {2110,425,210,12}, // topo do muro de separacao
  {5050,370,600,12} // patio da escola, no nivel do ultimo degrau
 };
 // Rota de retorno alta: somente acessivel a partir da margem direita.
 // O salto sobre o primeiro corrego continua obrigatorio na ida.
 // Duas plataformas elevadas permitem voltar ao muro e aos telhados.
 // A plataforma extra a direita facilita o avanco para o segundo corrego.
 std::vector<Rect> returnPlatforms={
  {2470,315,140,20}, // retorno alto: descer no muro usando A
  {2660,420,140,20}, // acesso pela caixa na margem direita
  {3195,411,145,20}  // apoio rumo ao segundo corrego
 };
 std::vector<Rect> solid={
  {1995,GROUND-64,64,64}, // caixa de acesso, no chao
  {2790,GROUND-64,64,64}  // caixa entre os corregos
 };
 // Degraus realmente solidos: a Laura nao atravessa lateralmente nem por baixo.
 // Cada degrau e uma coluna de alvenaria ate o fundo do terreno.
 std::vector<Rect> stairs;
 for(int i=0;i<8;i++)stairs.push_back({4510.f+i*76.f,615.f-i*35.f,76.f,720.f-(615.f-i*35.f)});
 const Rect schoolBase{5050,370,600,350};
 const Rect barrier{2110,437,210,178};
 // A grade em frente a casa continua decorativa e nao recebe colisao.
 std::array<Rect,3> movers{{{2880,442,145,22},{3540,475,145,22},{4110,510,145,22}}};
 float x=530,y=GROUND-35,vy=0,cam=0,runClock=0,jumpElapsed=0,landTimer=0,hurtTimer=0,fade=0,worldTime=0,peakFeet=GROUND,dropTimer=0;
 bool titleScreen=true;bool paused=false;int pauseChoice=0;float titleTime=0;bool grounded=true,facingLeft=false,collected=false,finished=false;int fadeState=0;bool running=true;int menuChoice=0;
 Uint64 prev=SDL_GetPerformanceCounter();double freq=double(SDL_GetPerformanceFrequency());
 bool bgmEnabled=true;
 auto setMusic=[&](int track,bool restart,bool playing=true){
  if(audio){SDL_LockAudioDevice(audio);mix.musicTrack=track;
   if(restart)mix.musicPos=0;
   mix.musicPlaying=playing && bgmEnabled;SDL_UnlockAudioDevice(audio);}
 };
 auto reset=[&](){x=530;y=GROUND-35;vy=0;grounded=true;collected=false;finished=false;cam=0;landTimer=hurtTimer=jumpElapsed=dropTimer=0;menuChoice=0;worldTime=0;setMusic(1,true);};
 auto goTitle=[&](){
  // Reset both stages and clear any queued voice/jingle before restoring the title BGM.
  reset();level2.reset();stage2=false;titleScreen=true;paused=false;pauseChoice=0;titleTime=0;fade=0;fadeState=0;
  if(audio){SDL_LockAudioDevice(audio);
   for(auto&v:mix.voices)v=Voice{};
   mix.musicTrack=0;mix.musicPos=0;mix.musicPlaying=bgmEnabled;
   SDL_UnlockAudioDevice(audio);}
 };
 while(appletMainLoop()&&running){
  Uint64 now=SDL_GetPerformanceCounter();float dt=std::min(float((now-prev)/freq),0.033f);prev=now;
  if(!titleScreen&&!paused)worldTime+=dt;
  SDL_Event ev;while(SDL_PollEvent(&ev))if(ev.type==SDL_QUIT)running=false;
  padUpdate(&pad);u64 down=padGetButtonsDown(&pad);
  // - fecha o aplicativo em qualquer tela, com encerramento normal do SDL.
  if(down&HidNpadButton_Minus){running=false;break;}
  if(titleScreen){
   titleTime+=dt;
   if(down&(HidNpadButton_A|HidNpadButton_B|HidNpadButton_Plus|HidNpadButton_X)){
    titleScreen=false;stage2=(down&HidNpadButton_X)!=0;
    reset();if(stage2){level2.reset();menuChoice=0;setMusic(2,true);}
    down=0; // Nao reutilizar Start como comando no mesmo quadro.
   }
   // Arte original enviada pelo usuario: exibida integralmente.
   SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
   if(titleTexture)SDL_RenderCopy(r,titleTexture,nullptr,nullptr);
   SDL_RenderPresent(r);
   continue;
  }
  // + opens/closes pause. While paused, D-pad selects, A confirms and B resumes.
  if(down&HidNpadButton_Plus){
   paused=!paused;pauseChoice=0;
   setMusic(stage2?2:1,false,!paused && (stage2?!level2.finished:!finished));
   down=0; // Do not let the same button also act on the game/menu.
  }
  if(paused){
   if(down&HidNpadButton_Up)pauseChoice=(pauseChoice+3)%4;
   if(down&HidNpadButton_Down)pauseChoice=(pauseChoice+1)%4;
   if(down&HidNpadButton_B){paused=false;setMusic(stage2?2:1,false,stage2?!level2.finished:!finished);down=0;}
   else if(down&HidNpadButton_A){
    if(pauseChoice==3){goTitle();continue;}
    if(pauseChoice==2){
     bgmEnabled=!bgmEnabled;
     setMusic(stage2?2:1,false,false);
     down=0;
    }else if(pauseChoice==1){
     if(stage2)level2.reset();
     else reset();
     paused=false;pauseChoice=0;
     setMusic(stage2?2:1,true,true);down=0;
    }else{
     paused=false;setMusic(stage2?2:1,false,stage2?!level2.finished:!finished);down=0;
    }
   }
  }
  // A segunda fase tem camera vertical, mapa, colisao e HUD independentes.
  // X na tela inicial abre diretamente a fase 2 para testes da branch.
  if(stage2){
   if(!paused){
    if(level2.finished){
     if(down&(HidNpadButton_Up|HidNpadButton_Down))menuChoice=1-menuChoice;
     if(down&(HidNpadButton_A|HidNpadButton_B|HidNpadButton_X)){
      if(menuChoice==0){level2.reset();setMusic(2,true);}
      else{goTitle();continue;} // No third stage yet; return to title.
     }
    }else{
     HidAnalogStickState s2Stick=padGetStickPos(&pad,0);
     float axis=float(s2Stick.x)/32767.f;
     level2.update(dt,down,axis,audio,mix);
    }
   }
   level2.render(r,mom,anim,frames);
   if(level2.finished)completeMenu(r,2,menuChoice);
   if(paused)pauseMenu(r,pauseChoice,bgmEnabled);
   SDL_RenderPresent(r);
   continue;
  }
  float h=0;
  if(!paused){
  HidAnalogStickState stick=padGetStickPos(&pad,0);h=float(stick.x)/32767.f;if(std::fabs(h)<.15f)h=0;
  if(h<0)facingLeft=true;else if(h>0)facingLeft=false;
  if(finished){
   // Menu de conclusao da fase 1: REINICIAR ou PROXIMO NIVEL.
   if(down&(HidNpadButton_Down|HidNpadButton_Up))menuChoice=1-menuChoice;
   if(down&(HidNpadButton_A|HidNpadButton_B)){
    if(menuChoice==0)reset();
    else{
     stage2=true;level2.reset();menuChoice=0;setMusic(2,true);
    }
   }
  }
  for(int i=0;i<3;i++){float origins[3]={2880,3540,4110};float ranges[3]={22,135,115};float speeds[3]={.85f,.95f,1.25f};movers[i].x=origins[i]+ranges[i]*std::sin(worldTime*speeds[i]+i);}
  if(fadeState==0&&!finished){
   dropTimer=std::max(0.f,dropTimer-dt);
   if((down&HidNpadButton_A)&&grounded){
    bool onOneWay=false;
    for(const auto& p:oneWay)if(std::fabs(y+35-p.y)<3&&x+26>p.x&&x-26<p.x+p.w)onOneWay=true;
    for(const auto& p:returnPlatforms)if(std::fabs(y+35-p.y)<3&&x+26>p.x&&x-26<p.x+p.w)onOneWay=true;
    for(const auto& p:movers)if(std::fabs(y+35-p.y)<3&&x+26>p.x&&x-26<p.x+p.w)onOneWay=true;
    if(onOneWay){dropTimer=.30f;grounded=false;vy=110;y+=9;landTimer=0;play(audio,mix,laura_effort,laura_effort_count,.35f);}
   }
   if((down&HidNpadButton_B)&&grounded&&hurtTimer<=0){vy=-650;grounded=false;jumpElapsed=0;peakFeet=y+35;play(audio,mix,laura_jump,laura_jump_count,.65f);}
   if(hurtTimer>0){hurtTimer=std::max(0.f,hurtTimer-dt);h=0;}
   if(landTimer>0)landTimer=std::max(0.f,landTimer-dt);
   float oldX=x,oldY=y;float nextX=std::max(35.f,std::min(WORLD-35.f,x+h*420*dt));
   Rect player{nextX-26,y-35,52,70};
   // Paredes de blocos solidos
   auto sideBlock=[&](const Rect& p){if(player.y+player.h>p.y+3&&player.y<p.y+p.h-3){if(nextX>oldX&&oldX+26<=p.x&&nextX+26>p.x)nextX=p.x-26;if(nextX<oldX&&oldX-26>=p.x+p.w&&nextX-26<p.x+p.w)nextX=p.x+p.w+26;}};
   for(const auto& p:solid)sideBlock(p);
   sideBlock(barrier);
   // Subida natural de degraus de 35 px: sobe ao encostar se estiver no chao.
   for(const auto& p:stairs){
    if(nextX+26>p.x&&nextX-26<p.x+p.w && oldX+26<=p.x+3 && y+35>p.y+2 && y+35<=p.y+39 && grounded && h>0){
     y=p.y-35;vy=0;grounded=true;
    }else sideBlock(p);
   }
   sideBlock(schoolBase);
   // Collision faces between pavement and the lowered water surface.
   const Rect canalBanks[4]={{2308,615,12,35},{2660,615,12,35},{3158,615,12,35},{4510,615,12,35}};
   for(const auto& bank:canalBanks)sideBlock(bank);
   x=nextX;
   if(!grounded){vy=std::min(1050.f,vy+1500.f*(vy>0?1.18f:1.f)*dt);y+=vy*dt;jumpElapsed+=dt;peakFeet=std::min(peakFeet,y+35);}
   bool landed=false;float landedY=GROUND;bool onMoving=false;int movingIndex=-1;
   auto landing=[&](const Rect&p,bool mobile,int idx){if(x+26<=p.x||x-26>=p.x+p.w)return;
    if(vy>=0 && oldY+35<=p.y+5 && y+35>=p.y){if(!landed||p.y<landedY){landed=true;landedY=p.y;onMoving=mobile;movingIndex=idx;}}
    if(vy<0 && oldY-35>=p.y+p.h && y-35<=p.y+p.h){y=p.y+p.h+35;vy=0;}
   };
   for(const auto&p:solid)landing(p,false,-1);
   for(const auto&p:stairs)landing(p,false,-1);
   landing(schoolBase,false,-1);
   if(dropTimer<=0)for(size_t pi=0;pi<returnPlatforms.size();++pi){
    const auto& p=returnPlatforms[pi];
    // Os dois apoios de retorno nao capturam saltos de quem avanca
    // da esquerda para a direita. Assim nao viram atalho sobre a agua.
    if(pi<2 && (h>=0 || oldX<=p.x))continue;
    if(x+26>p.x&&x-26<p.x+p.w&&vy>=0&&oldY+35<=p.y+5&&y+35>=p.y)
     if(!landed||p.y<landedY){landed=true;landedY=p.y;onMoving=false;}
   }
   if(dropTimer<=0)for(const auto&p:oneWay){
    if(x+26>p.x&&x-26<p.x+p.w&&vy>=0&&oldY+35<=p.y+5&&y+35>=p.y)
     if(!landed||p.y<landedY){landed=true;landedY=p.y;onMoving=false;}
   }
   if(dropTimer<=0)for(int i=0;i<3;i++)landing(movers[i],true,i);
   // solo somente nas regioes secas
   bool landRegion=(x<2320 || (x>=2660&&x<3170) || x>=4510);
   if(landRegion&&vy>=0&&oldY+35<=GROUND&&y+35>=GROUND){if(!landed||GROUND<landedY){landed=true;landedY=GROUND;onMoving=false;}}
   if(landed){float fall=landedY-peakFeet;y=landedY-35;peakFeet=landedY;vy=0;if(!grounded){if(fall>480){hurtTimer=.54f;play(audio,mix,sfx_damaged1,sfx_damaged1_count,.5f);}else landTimer=.24f;}grounded=true;
    if(onMoving&&movingIndex>=0){float speed[3]={.85f,.95f,1.25f};float range[3]={22,135,115};x+=range[movingIndex]*speed[movingIndex]*std::cos(worldTime*speed[movingIndex]+movingIndex)*dt;}
   }else{
    bool supported=landRegion&&std::fabs(y+35-GROUND)<2;
    for(const auto&p:solid)if(std::fabs(y+35-p.y)<2&&x+26>p.x&&x-26<p.x+p.w)supported=true;
    for(const auto&p:stairs)if(std::fabs(y+35-p.y)<2&&x+26>p.x&&x-26<p.x+p.w)supported=true;
    if(std::fabs(y+35-schoolBase.y)<2&&x+26>schoolBase.x&&x-26<schoolBase.x+schoolBase.w)supported=true;
    if(dropTimer<=0)for(const auto&p:returnPlatforms)if(std::fabs(y+35-p.y)<2&&x+26>p.x&&x-26<p.x+p.w)supported=true;
    if(dropTimer<=0)for(const auto&p:oneWay)if(std::fabs(y+35-p.y)<2&&x+26>p.x&&x-26<p.x+p.w)supported=true;
    for(int i=0;i<3;i++)if(dropTimer<=0&&std::fabs(y+35-movers[i].y)<2&&x+26>movers[i].x&&x-26<movers[i].x+movers[i].w){supported=true;float speed[3]={.85f,.95f,1.25f};float range[3]={22,135,115};x+=range[i]*speed[i]*std::cos(worldTime*speed[i]+i)*dt;}
    if(grounded&&!supported){grounded=false;vy=0;jumpElapsed=.2f;peakFeet=y+35;}
   }
   if(!collected&&std::fabs(x-804)<65&&std::fabs((y-35)-270)<80){collected=true;play(audio,mix,sfx_healed1,sfx_healed1_count,.75f);}
   // Agua causa falha, jacare apenas visual
   bool water=((x+20>2320&&x-20<2660)||(x+20>3170&&x-20<4510));
   if(water&&y+35>=650){fadeState=1;fade=0;play(audio,mix,sfx_water,sfx_water_count,.8f);}
   // A porta exige Y, como a interacao da fase 2. O coelho continua obrigatorio.
   if(collected && (down&HidNpadButton_Y) && x+26>5270 && x-26<5382 && y+35>=364 && y+35<=380){
    finished=true;if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=false;SDL_UnlockAudioDevice(audio);}play(audio,mix,sfx_complete,sfx_complete_count,.85f);
   }
  }
  if(fadeState==1){fade=std::min(1.f,fade+dt/.4f);if(fade>=1){reset();fadeState=2;}}
  else if(fadeState==2){fade=std::max(0.f,fade-dt/.4f);if(fade<=0)fadeState=0;}
  if(grounded&&std::fabs(h)>.1f)runClock+=dt*12;else runClock=0;
  cam+=((std::max(0.f,std::min(WORLD-SW,x-410)))-cam)*std::min(1.f,dt*6);
  } // End gameplay updates: all motion, collision, timers and input freeze on pause.
  scene(r,cam,worldTime,solid,movers,returnPlatforms,collected);
  // Indicador de objetivo (sem dependencia de fonte)
  fill(r,18,18,375,42,32,57,65);label(r,collected?"VA PARA A ESCOLA":"PEGUE O COELHO",35,28,3,250,240,214,0);
  if(!finished && collected && x+26>5270 && x-26<5382 && y+35>=364 && y+35<=380){
   fill(r,390,77,500,46,40,81,86);label(r,"APERTE Y PARA ENTRAR",423,89,3,255,244,213,0);
  }
  int selected=0;
  if(hurtTimer>0)selected=15+std::min(2,int((.54f-hurtTimer)/.18f));
  else if(!grounded){if(jumpElapsed<.09f)selected=7;else if(vy<-90)selected=8;else if(vy<90)selected=9;else selected=10;}
  else if(landTimer>0)selected=11+std::min(3,int((.24f-landTimer)/.06f));
  else if(std::fabs(h)>.1f)selected=1+int(runClock*1.2f)%6;
  constexpr int padding[18]={3,2,4,1,7,5,2,27,8,15,11,47,34,20,24,30,30,20};
  float spriteH=85,spriteW=spriteH*frames[selected]->w/frames[selected]->h;
  SDL_Rect dest{int(x-cam-spriteW/2),int(y+35-spriteH+spriteH*padding[selected]/frames[selected]->h),int(spriteW),int(spriteH)};
  // Paleta avermelhada APENAS no primeiro frame de dano, sem alterar PNG.
  if(selected==15)SDL_SetTextureColorMod(anim[selected],255,174,174);
  SDL_RenderCopyEx(r,anim[selected],nullptr,&dest,0,nullptr,facingLeft?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
  if(selected==15)SDL_SetTextureColorMod(anim[selected],255,255,255);
  if(finished)completeMenu(r,1,menuChoice);
  if(fade>0){SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,0,0,0,Uint8(fade*255));SDL_Rect full{0,0,SW,SH};SDL_RenderFillRect(r,&full);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);}
  if(paused)pauseMenu(r,pauseChoice,bgmEnabled);
  SDL_RenderPresent(r);
 }
 if(audio)SDL_CloseAudioDevice(audio);for(auto&t:pauseTextures)if(t)SDL_DestroyTexture(t);for(auto&t:completeTextures)if(t)SDL_DestroyTexture(t);for(auto&t:terrainTextures)if(t)SDL_DestroyTexture(t);if(titleTexture)SDL_DestroyTexture(titleTexture);for(auto&t:mom)if(t)SDL_DestroyTexture(t);for(auto&t:anim)SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
