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
#include "title_art.h"

constexpr int SW=1280, SH=720;
constexpr float WORLD=5650.0f, GROUND=615.0f;
struct Voice {const int16_t* data=nullptr; int len=0,pos=0; float volume=1;};
struct Mixer { Voice voices[8]; bool musicPlaying=true; int musicTrack=0; int musicPos=0; float musicVolume=0.25f; };
static void audioCallback(void* p, Uint8* stream,int len) {
 Mixer& m=*static_cast<Mixer*>(p); auto* dst=(int16_t*)stream;
 for(int i=0;i<len/2;++i){int sum=0;
  for(auto& v:m.voices) if(v.data && v.pos<v.len){sum+=int(v.data[v.pos++]*v.volume);if(v.pos==v.len)v.data=nullptr;}
  // Both original soundtracks are 24 kHz mono; output is 48 kHz mono.
  if(m.musicPlaying){
   const int8_t* track=m.musicTrack==0?lauras_sunset_pcm:lauras_theme_pcm;
   const int count=m.musicTrack==0?lauras_sunset_count:lauras_theme_count;
   if(count>1){
    int idx=(m.musicPos/2)%count;
    int next=(idx+1)%count;
    int value=(m.musicPos&1)?(int(track[idx])+int(track[next]))/2:int(track[idx]);
    sum+=int(value*256*m.musicVolume);
    m.musicPos=(m.musicPos+1)%(count*2);
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
static void building(SDL_Renderer*r,int x,int top,int w,int h,Uint8 cr,Uint8 cg,Uint8 cb,int cam){
 fill(r,x,top,w,h,cr,cg,cb,cam);fill(r,x,top,w,9,98,66,52,cam);
 for(int i=0;i<w/80;i++){int wx=x+25+i*80;fill(r,wx,top+45,39,52,65,79,84,cam);fill(r,wx+4,top+49,31,44,122,166,176,cam);fill(r,wx+18,top+49,3,44,54,63,66,cam);}
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
 for(int i=0;i<34;i++){int px=i*175-int(cam*.43f);fill(r,px,400+(i%3)*14,118,GROUND-(400+(i%3)*14),204,167,134);fill(r,px-8,389+(i%3)*14,132,17,152,79,62);
  fill(r,px+137,390,9,GROUND-390,93,67,46);fill(r,px+103,344,76,55,58,122,74);fill(r,px+114,327,53,45,72,137,80);}
 // Vila
 building(r,0,312,460,303,88,144,143,int(cam));building(r,460,310,585,305,206,164,111,int(cam));
 fill(r,12,362,430,40,36,102,111,int(cam));label(r,"MERCADO",92,371,4,245,213,151,int(cam));
 fill(r,470,358,563,41,180,99,67,int(cam));label(r,"PADARIA",620,366,4,255,222,166,int(cam));
 // Vitrines, mercadorias e pessoas (figuras simples)
 fill(r,60,428,335,145,49,72,78,int(cam));for(int i=0;i<4;i++){fill(r,68,446+i*29,320,4,140,100,61,int(cam));for(int j=0;j<9;j++)fill(r,77+j*34,429+i*29,19,16,194+(i%2)*30,136,62,int(cam));}
 for(int i=0;i<2;i++){int px=270+i*82;fill(r,px,474,21,26,194,135,105,int(cam));fill(r,px-7,499,35,47,66,93,120,int(cam));}
 fill(r,535,450,250,116,129,190,195,int(cam));fill(r,545,460,230,98,193,221,219,int(cam));fill(r,540,525,240,12,115,73,49,int(cam));for(int i=0;i<8;i++)fill(r,552+i*27,489+(i%2)*13,22,18,206,145,71,int(cam));
 fill(r,815,433,94,145,71,101,109,int(cam));for(int i=0;i<3;i++)fill(r,825,448+i*37,75,27,190,218,220,int(cam));
 // telhas da padaria e mercado
 for(int i=0;i<26;i++)fill(r,i*42,299+(i%2)*3,40,14,144+(i%3)*12,68,51,int(cam));
 // Rua e muros de casas
 building(r,1270,277,320,338,180,157,133,int(cam));building(r,1680,335,430,280,191,166,137,int(cam));
 fill(r,1490,427,590,188,146,137,120,int(cam));for(int i=0;i<16;i++)fill(r,1495+i*36,438+(i%3)*11,24,3,117,108,96,int(cam));
 // Muro de separacao: chega ate o solo e bloqueia acesso lateral ao corrego.
 fill(r,2110,430,210,185,140,132,113,int(cam));
 fill(r,2110,425,210,12,187,174,147,int(cam));
 for(int i=0;i<5;i++)fill(r,2120+i*40,470+(i%2)*24,26,4,111,105,91,int(cam));
 // grade: vazada, NAO e solida
 for(int i=0;i<9;i++)fill(r,1445+i*16,435,5,180,63,69,68,int(cam));
 fill(r,1440,435,152,9,55,61,59,int(cam));
 // Calçadas e beiradas dos corrego
 auto ground=[&](float a,float b,float top){fill(r,a,top,b-a,SH-top,86,85,76,int(cam));fill(r,a,top,b-a,13,171,167,147,int(cam));for(int i=int(a);i<b;i+=54)line(r,i,int(top+13),i+12,int(top+13),61,64,62,int(cam));};
 ground(0,2320,615);ground(2660,3170,615);ground(4510,5650,615);
 // agua dos dois corregos
 for(int n=0;n<2;n++){int a=n==0?2320:3170,b=n==0?2660:4510;
  fill(r,a,618,b-a,SH-618,53,101,77,int(cam));for(int i=a;i<b;i+=45){int wig=int(3*std::sin(time*2+i*.08));fill(r,i,624+wig,25,3,119,157,114,int(cam));}
 }
 // Jacare animado: corpo ondulante, cauda, patas, focinho, dentes e olho.
 float crocX=3730+145*std::sin(time*.72f), bob=3*std::sin(time*2.8f);
 float tail=22*std::sin(time*3.1f);
 fill(r,crocX-65,681+tail*.25f,82,13,62,99,46,int(cam));
 fill(r,crocX-94,686+tail*.45f,34,7,59,94,42,int(cam));
 fill(r,crocX,666+bob,177,30,66,112,47,int(cam));
 for(int j=0;j<8;j++)fill(r,crocX+10+j*20,661+bob+(j%2)*3,12,8,48,85,39,int(cam));
 fill(r,crocX+23,691+bob,24,10,50,89,39,int(cam));
 fill(r,crocX+130,691+bob,23,10,50,89,39,int(cam));
 fill(r,crocX+160,654+bob,47,32,75,127,51,int(cam));
 fill(r,crocX+191,672+bob,60,14,78,126,50,int(cam));
 fill(r,crocX+194,682+bob,54,6,51,77,40,int(cam));
 for(int j=0;j<5;j++)fill(r,crocX+199+j*10,680+bob,5,5,240,235,199,int(cam));
 fill(r,crocX+183,661+bob,9,8,248,242,190,int(cam));
 fill(r,crocX+187,662+bob,3,4,26,34,22,int(cam));
 fill(r,crocX+238,672+bob,5,4,37,64,29,int(cam));
 // Escadaria solida visualmente: degraus apoiados no terreno e colisoes no topo.
 for(int i=0;i<8;i++){float xx=4510+i*76, yy=615-i*35;fill(r,xx,yy,80,SH-yy,96,97,88,int(cam));fill(r,xx,yy,80,9,178,176,150,int(cam));}
 // A escola tem alicerce ate o solo, sem vazio embaixo.
 fill(r,5050,370,600,SH-370,96,97,88,int(cam));
 fill(r,5050,370,600,10,178,176,150,int(cam));
 building(r,5050,165,600,205,113,167,113,int(cam));fill(r,5060,175,575,40,79,132,86,int(cam));
 // arco iris simplificado em faixas
 for(int i=0;i<5;i++){Uint8 cols[5][3]={{220,86,80},{230,161,70},{220,208,84},{95,175,111},{96,137,204}};fill(r,5200+i*11,190+i*8,275-i*22,8,cols[i][0],cols[i][1],cols[i][2],int(cam));}
 label(r,"ESCOLA ARCO-IRIS",5120,264,3,239,244,220,int(cam));fill(r,5270,286,112,84,35,80,69,int(cam));fill(r,5321,286,5,84,181,204,187,int(cam));
 // Caixas e superficie do muro; telhados usam a propria arte das construcoes.
 for(const auto& b:blocks){
  if(b.w==64&&b.h==64)crate(r,b.x,b.y,int(cam));
 }
 for(const auto& p:returnPlatforms){fill(r,p.x,p.y,p.w,p.h,111,132,145,int(cam));fill(r,p.x,p.y,p.w,6,176,207,207,int(cam));fill(r,p.x+9,p.y+10,p.w-18,4,64,93,109,int(cam));}
 for(auto& p:moving){fill(r,p.x,p.y,p.w,p.h,82,125,152,int(cam));fill(r,p.x,p.y,p.w,6,139,200,213,int(cam));fill(r,p.x+8,p.y+10,p.w-16,5,48,82,98,int(cam));}
 if(!collected)rabbit(r,780,240,int(cam));
}
#include "level2.h"

int main(int,char**){
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)!=0)return 1;
 SDL_Window* win=SDL_CreateWindow("Laura Adventure 1.2 - Fase 2",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,SW,SH,SDL_WINDOW_SHOWN);
 if(!win){SDL_Quit();return 2;}
 SDL_Renderer* r=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
 if(!r)r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);
 if(!r){SDL_DestroyWindow(win);SDL_Quit();return 3;}
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
 bool titleScreen=true;bool paused=false;float titleTime=0;bool grounded=true,facingLeft=false,collected=false,finished=false;int fadeState=0;bool running=true;int menuChoice=0;
 Uint64 prev=SDL_GetPerformanceCounter();double freq=double(SDL_GetPerformanceFrequency());
 auto reset=[&](){x=530;y=GROUND-35;vy=0;grounded=true;collected=false;finished=false;cam=0;landTimer=hurtTimer=jumpElapsed=dropTimer=0;menuChoice=0;worldTime=0;if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=true;mix.musicTrack=1;mix.musicPos=0;SDL_UnlockAudioDevice(audio);}};
 while(appletMainLoop()&&running){
  Uint64 now=SDL_GetPerformanceCounter();float dt=std::min(float((now-prev)/freq),0.033f);prev=now;
  if(!titleScreen&&!paused)worldTime+=dt;
  SDL_Event ev;while(SDL_PollEvent(&ev))if(ev.type==SDL_QUIT)running=false;
  padUpdate(&pad);u64 down=padGetButtonsDown(&pad);
  if(titleScreen){
   titleTime+=dt;
   if(down&(HidNpadButton_A|HidNpadButton_B|HidNpadButton_Plus|HidNpadButton_X)){
    titleScreen=false;stage2=(down&HidNpadButton_X)!=0;
    reset();if(stage2)level2.reset();
    down=0; // Nao reutilizar Start como comando no mesmo quadro.
   }
   // Arte original enviada pelo usuario: exibida integralmente.
   SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
   if(titleTexture)SDL_RenderCopy(r,titleTexture,nullptr,nullptr);
   SDL_RenderPresent(r);
   continue;
  }
  // + abre a aventura na tela inicial; durante o jogo nao encerra o aplicativo.
  // Para sair, use HOME do Switch.
  // + alterna pause e jogo, inclusive apos concluir a fase.
  if(down&HidNpadButton_Plus){
   paused=!paused;
   if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=!paused && (stage2 ? !level2.finished : !finished);SDL_UnlockAudioDevice(audio);}
  }
  // A segunda fase tem camera vertical, mapa, colisao e HUD independentes.
  // X na tela inicial abre diretamente a fase 2 para testes da branch.
  if(stage2){
   if(!paused){
    if(level2.finished&&(down&HidNpadButton_X)){
     level2.reset();
     if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=true;mix.musicTrack=1;mix.musicPos=0;SDL_UnlockAudioDevice(audio);}
    }else{
     HidAnalogStickState s2Stick=padGetStickPos(&pad,0);
     float axis=float(s2Stick.x)/32767.f;
     level2.update(dt,down,axis,audio,mix);
    }
   }
   level2.render(r,mom,anim,frames);
   if(paused){
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,8,16,32,160);
    SDL_Rect veil{0,0,SW,SH};SDL_RenderFillRect(r,&veil);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    fill(r,395,270,490,170,25,52,78);fill(r,404,279,472,152,54,95,115);
    label(r,"PAUSE",475,320,11,255,238,178,0);
    label(r,"START PARA VOLTAR",467,402,2,245,236,216,0);
   }
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
     stage2=true;level2.reset();
     if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=true;mix.musicTrack=1;mix.musicPos=0;SDL_UnlockAudioDevice(audio);}
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
   bool water=((x>=2320&&x<2660)||(x>=3170&&x<4510));
   if(water&&y+35>=618){fadeState=1;fade=0;play(audio,mix,sfx_water,sfx_water_count,.8f);}
   // Conclusao exige tocar a porta, com o coelho coletado e sobre o patio.
   if(collected && x+26>5270 && x-26<5382 && y+35>=364 && y+35<=380){
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
  if(finished){
   fill(r,265,184,750,340,27,75,67);
   fill(r,277,196,726,316,48,108,93);
   label(r,"MISSAO CONCLUIDA",326,228,5,250,233,180,0);
   label(r,"LAURA CHEGOU A ESCOLA",348,302,3,250,233,180,0);
   fill(r,345,367,590,48,menuChoice==0?229:70,menuChoice==0?196:113,menuChoice==0?114:102);
   label(r,"REINICIAR",474,377,4,menuChoice==0?36:217,menuChoice==0?67:230,menuChoice==0?59:209,0);
   fill(r,345,430,590,48,menuChoice==1?105:67,menuChoice==1?117:92,menuChoice==1?109:86);
   label(r,"PROXIMO NIVEL",405,441,3,menuChoice==1?251:182,menuChoice==1?239:193,menuChoice==1?183:167,0);
   // Fase 2 agora selecionavel; nao mostrar cadeado.
  }
  if(fade>0){SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,0,0,0,Uint8(fade*255));SDL_Rect full{0,0,SW,SH};SDL_RenderFillRect(r,&full);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);}
  if(paused){
   SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(r,8,16,32,160);
   SDL_Rect veil{0,0,SW,SH};SDL_RenderFillRect(r,&veil);
   SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
   fill(r,395,270,490,170,25,52,78);
   fill(r,404,279,472,152,54,95,115);
   label(r,"PAUSE",475,320,11,255,238,178,0);
   label(r,"START PARA VOLTAR",467,402,2,245,236,216,0);
  }
  SDL_RenderPresent(r);
 }
 if(audio)SDL_CloseAudioDevice(audio);if(titleTexture)SDL_DestroyTexture(titleTexture);for(auto&t:mom)if(t)SDL_DestroyTexture(t);for(auto&t:anim)SDL_DestroyTexture(t);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
