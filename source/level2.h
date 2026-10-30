#pragma once
// Prototipo jogavel da Fase 2 - O Caminho das Nuvens.
// Usa o mesmo renderer, fisica 60 fps com dt e sprites da Laura da Fase 1.
// Todas as alturas das plataformas indicam a SUPERFICIE de aterrissagem.
#include "mamae_frames.h"

static constexpr float L2_W = 5100.f, L2_GROUND = 1490.f;
struct Cloud2 { Rect r; float origin, amplitude, speed, phase; bool moving; };
static void l2box(SDL_Renderer*r,float x,float y,float w,float h,int R,int G,int B,float cx,float cy){
 SDL_SetRenderDrawColor(r,(Uint8)R,(Uint8)G,(Uint8)B,255);
 SDL_Rect q{int(x-cx),int(y-cy),std::max(1,int(w)),std::max(1,int(h))}; SDL_RenderFillRect(r,&q);
}
static void l2ellipse(SDL_Renderer*r,float cx,float cy,float rx,float ry,int R,int G,int B,float cameraX,float cameraY){
 SDL_SetRenderDrawColor(r,(Uint8)R,(Uint8)G,(Uint8)B,255);
 for(int y=-int(ry);y<=int(ry);++y){
  float v=1.f-(y*y)/(ry*ry);if(v<0)continue;
  int hw=int(rx*std::sqrt(v));SDL_RenderDrawLine(r,int(cx-hw-cameraX),int(cy+y-cameraY),int(cx+hw-cameraX),int(cy+y-cameraY));
 }
}
static void l2label(SDL_Renderer*r,const char*s,int x,int y,int scale,int R,int G,int B,float cx,float cy){
 label(r,s,int(x-cx),int(y-cy),scale,(Uint8)R,(Uint8)G,(Uint8)B,0);
}
static void l2cloud(SDL_Renderer*r,Rect p,float cx,float cy,bool moving){
 const float x=p.x,y=p.y,w=p.w;
 l2ellipse(r,x+w*.50f,y+8,w*.50f,16,117,187,199,cx,cy);
 l2ellipse(r,x+w*.50f,y-4,w*.49f,20,242,252,254,cx,cy);
 l2ellipse(r,x+w*.27f,y-13,w*.21f,25,250,254,255,cx,cy);
 l2ellipse(r,x+w*.63f,y-18,w*.26f,31,255,255,255,cx,cy);
 l2ellipse(r,x+w*.80f,y-6,w*.16f,21,249,254,255,cx,cy);
 if(moving){l2box(r,x+w*.41f,y-12,10,5,255,204,144,cx,cy);}
}
static void l2house(SDL_Renderer*r,float x,float roof,float w,float bottom,int R,int G,int B,float cx,float cy){
 float top=roof+11;
 l2box(r,x,top,w,bottom-top,R,G,B,cx,cy);
 l2box(r,x-10,roof,w+20,12,121,70,63,cx,cy);
 for(int j=0;j<int(w/95);++j){
  float wx=x+27+j*92;
  l2box(r,wx,top+42,39,49,59,111,128,cx,cy);l2box(r,wx+4,top+46,31,41,147,215,220,cx,cy);
  l2box(r,wx+19,top+46,3,41,83,123,131,cx,cy);
 }
 for(int k=0;k<int(w/31)+1;++k)l2box(r,x-10+k*31,roof+2,28,5,166,101,81,cx,cy);
}
static void l2capy(SDL_Renderer*r,float x,float feet,float cx,float cy){
 // Capivara de pelucia provisoria. Substituivel por PNG sem alterar a coleta.
 l2ellipse(r,x,feet-18,28,22,153,103,69,cx,cy);
 l2ellipse(r,x+17,feet-29,23,20,190,137,92,cx,cy);
 l2ellipse(r,x+5,feet-47,7,9,137,91,63,cx,cy);
 l2ellipse(r,x+29,feet-47,7,9,137,91,63,cx,cy);
 l2box(r,x+29,feet-31,5,5,45,41,41,cx,cy);
 l2box(r,x+6,feet-34,4,4,47,40,36,cx,cy);
 l2box(r,x+21,feet-22,10,6,108,69,52,cx,cy);
 l2box(r,x-17,feet-6,16,8,132,91,65,cx,cy);
 l2box(r,x+8,feet-6,16,8,132,91,65,cx,cy);
 l2box(r,x-23,feet-2,8,3,236,211,175,cx,cy);
}

struct Level2 {
 float x=385,feet=L2_GROUND,vy=0,camX=0,camY=910,clock=0,runClock=0,jumpElapsed=0,dropTimer=0,peakFeet=L2_GROUND;
 float fade=0,interactionTimer=0;
 int fadeState=0,interaction=0; // 0 esperando, 1 exclama, 2 coracao, 3 retornando
 bool grounded=true,faceLeft=false,collected=false,finished=false;
 std::vector<Rect> walls,roofs,branches,stairs,ground;
 std::vector<Cloud2> clouds;
 Level2(){
  ground={{0,L2_GROUND,690,300},{870,L2_GROUND,640,300},{3230,L2_GROUND,1870,300}};
  // Muros fisicamente atravessaveis por baixo, firmes por cima.
  walls={{1190,1335,220,15},{980,1270,220,15},{770,1205,220,15},{570,1140,210,15}};
  roofs={{270,1080,320,15},{3340,1010,490,16}};
  branches={{2390,920,310,15},{2390,795,310,15},{2390,455,415,16}};
  // Nuvens iniciais, travessia acima da agua e subida apos galhos.
  auto cloud=[&](float xx,float yy,float ww,float amp,float speed,float phase){clouds.push_back({{xx,yy,ww,15},xx,amp,speed,phase,amp>0});};
  cloud(385,975,150,0,0,0);cloud(555,875,150,0,0,0);cloud(725,790,145,0,0,0);
  cloud(885,865,160,0,0,0);cloud(1080,950,185,85,.95f,0);
  cloud(1325,900,160,0,0,0);cloud(1550,855,180,70,.84f,1.1f);
  cloud(1790,895,155,0,0,0);cloud(1990,885,170,78,1.05f,.65f);
  cloud(2195,900,155,0,0,0);
  cloud(2730,710,155,65,.92f,1.5f);cloud(2790,615,155,0,0,0);cloud(2620,555,165,0,0,0);
  // Nuvens em escada, descida com A. Ordem fisica x cresce, y aumenta.
  cloud(2810,545,170,0,0,0);cloud(2940,655,170,46,.70f,2);
  cloud(3065,770,175,0,0,0);cloud(3195,880,175,58,.78f,.4f);
  // Toldos do predio da autoescola: mao unica e descida com A.
  roofs.push_back({3920,1120,170,13});roofs.push_back({4030,1240,170,13});roofs.push_back({4140,1360,170,13});
  // A escadaria liga o piso da rua a porta de vidro do consultorio.
  for(int i=0;i<5;++i)stairs.push_back({3510.f+i*75,1250.f+i*48,75.f,240.f-i*48});
 }
 void reset(){x=385;feet=L2_GROUND;vy=0;camX=0;camY=910;clock=0;runClock=0;jumpElapsed=0;dropTimer=0;peakFeet=L2_GROUND;fade=0;fadeState=0;interaction=0;interactionTimer=0;grounded=true;faceLeft=false;collected=false;finished=false;}
 bool xhit(const Rect&p)const{return x+24>p.x&&x-24<p.x+p.w;}
 bool onPlatform(const Rect&p)const{return xhit(p)&&std::fabs(feet-p.y)<3;}
 bool validSupport()const{
  for(const auto&p:ground)if(onPlatform(p))return true;
  for(const auto&p:walls)if(onPlatform(p))return true;
  for(const auto&p:roofs)if(onPlatform(p))return true;
  for(const auto&p:branches)if(onPlatform(p))return true;
  for(const auto&p:stairs)if(onPlatform(p))return true;
  if(onPlatform({1310,1426,64,64}))return true;
  for(const auto&p:clouds)if(onPlatform(p.r))return true;
  return false;
 }
 // Dano por cair no buraco/agua ou de altura muito grande, com fade preto.
 void die(SDL_AudioDeviceID audio,Mixer& mix,bool water){
  if(fadeState!=0)return;
  fadeState=1;fade=0;
  play(audio,mix,water?sfx_water:sfx_damaged1,water?sfx_water_count:sfx_damaged1_count,.75f);
 }
 void update(float dt,u64 down,float axis,SDL_AudioDeviceID audio,Mixer& mix){
  clock+=dt;
  if(fadeState){
   if(fadeState==1){fade=std::min(1.f,fade+dt/.36f);if(fade>=1){reset();fade=1;fadeState=2;}}
   else{fade=std::max(0.f,fade-dt/.42f);if(fade<=0)fadeState=0;}
   return;
  }
  // Durante a fala, o gameplay fica congelado. A primeira fase continua intocada.
  if(interaction){
   interactionTimer+=dt;
   if(interaction==1&&interactionTimer>.75f){interaction=2;interactionTimer=0;}
   else if(interaction==2&&interactionTimer>1.40f){interaction=3;interactionTimer=0;}
   else if(interaction==3&&interactionTimer>.55f){interaction=0;interactionTimer=0;finished=true;
    if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=false;SDL_UnlockAudioDevice(audio);}
    play(audio,mix,sfx_complete,sfx_complete_count,.85f);
   }
   return;
  }
  if(finished)return;
  // Mom está na porta, o Y só funciona depois da coleta da capivarinha.
  if((down&HidNpadButton_Y)&&collected&&std::fabs(x-3525)<118&&std::fabs(feet-1250)<96){
   interaction=1;interactionTimer=0;return;
  }
  if(axis<-.15f)faceLeft=true;else if(axis>.15f)faceLeft=false;
  if(std::fabs(axis)<.15f)axis=0;
  dropTimer=std::max(0.f,dropTimer-dt);
  // Plataforma movimenta Laura junto para nao escorregar no repouso.
  for(auto&c:clouds){
   if(!c.moving)continue;
   bool on=grounded&&onPlatform(c.r);
   float old=c.r.x;
   c.r.x=c.origin+c.amplitude*std::sin(clock*c.speed+c.phase);
   if(on)x+=c.r.x-old;
  }
  if(grounded&&(down&HidNpadButton_A)){
   bool passable=false;
   for(const auto&p:walls)passable|=onPlatform(p);
   for(const auto&p:roofs)passable|=onPlatform(p);
   for(const auto&p:branches)passable|=onPlatform(p);
   for(const auto&c:clouds)passable|=onPlatform(c.r);
   if(passable){dropTimer=.16f;grounded=false;feet+=8;vy=160;play(audio,mix,laura_effort,laura_effort_count,.3f);}
  }
  if(grounded&&(down&HidNpadButton_B)){grounded=false;vy=-650;peakFeet=feet;jumpElapsed=0;play(audio,mix,laura_jump,laura_jump_count,.6f);}
  float oldX=x,oldFeet=feet;
  float nx=std::max(32.f,std::min(L2_W-32.f,x+axis*420*dt));
  // Caixa de madeira e grade realmente solidas nas laterais.
  auto side=[&](Rect p){
   if(feet<=p.y+2||feet-68>=p.y+p.h-2)return;
   if(nx>oldX&&oldX+24<=p.x+1&&nx+24>p.x)nx=p.x-24;
   if(nx<oldX&&oldX-24>=p.x+p.w-1&&nx-24<p.x+p.w)nx=p.x+p.w+24;
  };
  side({1310,1426,64,64});side({1475,1200,28,290});
  // Degraus sobem naturalmente ao andar para a esquerda.
  for(auto&p:stairs){
   if(nx<oldX&&oldX-24>=p.x+p.w-2&&nx-24<p.x+p.w &&grounded&&feet>p.y&&feet<=p.y+54){
    feet=p.y;vy=0;grounded=true;
   }else side(p);
  }
  x=nx;
  if(!grounded){vy=std::min(1050.f,vy+1500.f*(vy>0?1.13f:1.f)*dt);feet+=vy*dt;jumpElapsed+=dt;peakFeet=std::min(peakFeet,feet);}
  bool landed=false;float best=1e10f;
  auto land=[&](const Rect&p,bool passable){
   if(!xhit(p)||vy<0)return;
   if(passable&&dropTimer>0)return;
   if(oldFeet<=p.y+5&&feet>=p.y&&p.y<best){landed=true;best=p.y;}
  };
  for(const auto&p:ground)land(p,false);
  for(const auto&p:stairs)land(p,false);
  land({1310,1426,64,64},false);
  for(const auto&p:walls)land(p,true);
  for(const auto&p:roofs)land(p,true);
  for(const auto&p:branches)land(p,true);
  for(const auto&c:clouds)land(c.r,true);
  if(landed){
   float fall=best-peakFeet;
   feet=best;vy=0;grounded=true;peakFeet=best;
   if(fall>475 && best>=L2_GROUND-5){die(audio,mix,false);return;}
  }else if(grounded&&!validSupport()){grounded=false;vy=0;peakFeet=feet;}
  if(!collected&&grounded&&std::fabs(x-2530)<68&&std::fabs(feet-455)<10){collected=true;play(audio,mix,sfx_healed1,sfx_healed1_count,.8f);}
  bool gap=x>690&&x<870;
  bool water=x>1508&&x<3230;
  if(feet>=L2_GROUND+12&&(gap||water)){die(audio,mix,water);return;}
  if(feet>1650){die(audio,mix,false);return;}
  if(grounded&&std::fabs(axis)>.15f)runClock+=dt*11;else runClock=0;
  camX+=(std::max(0.f,std::min(L2_W-SW,x-420))-camX)*std::min(1.f,dt*5.5f);
  camY+=(std::max(0.f,std::min(930.f,feet-575))-camY)*std::min(1.f,dt*4.5f);
 }
 void render(SDL_Renderer*r,SDL_Texture* mom[6],SDL_Texture* laura[18],const FrameData* frames[18])const{
  float cx=camX,cy=camY;
  SDL_SetRenderDrawColor(r,122,218,232,255);SDL_RenderClear(r);
  // Distancia e paralaxe das montanhas (coordenadas de mundo).
  for(int i=-1;i<17;++i){
   float px=i*440-cx*.16f;
   l2ellipse(r,px+150,870-cy*.08f,260,195,130,224,209,0,0);
   l2ellipse(r,px+240,1010-cy*.07f,285,175,100,205,183,0,0);
   l2ellipse(r,px+70,1160-cy*.04f,250,215,64,191,148,0,0);
  }
  // Casas e predios ao fundo: sem colisao e com paralaxe suave.
  for(int i=0;i<23;++i){
   float bx=i*225+50-cx*.26f, by=1150-cy*.18f+(i%3)*37;
   l2box(r,bx,by,105,320,221,200,165,0,0);
   l2box(r,bx-6,by-18,117,24,168,102,83,0,0);
   for(int k=0;k<3;k++)l2box(r,bx+22+(k%2)*44,by+33+k*60,27,37,110,176,187,0,0);
  }
  // Grama e terra, com pontos naturais de borda.
  for(const auto&p:ground){
   l2box(r,p.x,p.y,p.w,340,116,86,61,cx,cy);
   l2box(r,p.x,p.y,p.w,15,87,162,77,cx,cy);
   l2box(r,p.x,p.y-8,p.w,11,123,207,93,cx,cy);
   for(int i=0;i<int(p.w)/46;i++)l2box(r,p.x+i*46+9,p.y+42+(i%3)*31,14,9,91,65,52,cx,cy);
  }
  // Buraco de queda e agua proibida depois da grade.
  l2box(r,690,1490,180,190,38,92,97,cx,cy);
  l2box(r,1510,1490,1720,190,47,143,162,cx,cy);
  for(int i=0;i<38;i++){
   float xx=1515+i*46;float wob=5*std::sin(clock*2+i*.8f);
   l2box(r,xx,1500+wob,25,3,158,232,229,cx,cy);
  }
  // Condominio da abertura e percurso por cima dos muros.
  l2house(r,270,1080,320,1490,224,188,155,cx,cy);
  l2label(r,"CONDOMINIO",307,1173,2,88,77,75,cx,cy);
  for(const auto&p:walls){
   l2box(r,p.x,p.y,p.w,1500-p.y,171,151,123,cx,cy);
   l2box(r,p.x,p.y-7,p.w,13,208,185,149,cx,cy);
   for(int i=0;i<int(p.w)/48;i++)l2box(r,p.x+9+i*48,p.y+28+(i%2)*22,27,4,131,113,92,cx,cy);
  }
  // Caixa apoiada no solo e grade que bloqueia passagem para a agua.
  l2box(r,1310,1426,64,64,103,66,43,cx,cy);
  l2box(r,1315,1431,54,54,178,120,74,cx,cy);
  l2box(r,1339,1431,5,54,111,77,50,cx,cy);
  for(int i=0;i<7;i++)l2box(r,1475+i*4,1200,2,290,86,95,101,cx,cy);
  l2box(r,1471,1200,36,13,78,88,95,cx,cy);
  l2box(r,1471,1330,36,9,78,88,95,cx,cy);
  // Nuvens fisicas - brancas, redondas, com sombra azulada.
  for(const auto&cl:clouds)l2cloud(r,cl.r,cx,cy,cl.moving);
  // Arvore gigante com dois galhos e copa coletavel.
  l2box(r,2540,580,88,910,131,99,67,cx,cy);
  l2box(r,2560,640,16,800,164,121,78,cx,cy);
  for(int i=0;i<2;i++){
   const auto&p=branches[i];
   l2box(r,p.x,p.y,p.w,14,134,92,56,cx,cy);
   l2box(r,p.x,p.y-8,p.w,9,172,126,82,cx,cy);
  }
  for(int i=0;i<8;i++){
   float tx=2350+i*68;
   l2ellipse(r,tx,448+(i%3)*10,116,75,62+(i%3)*9,151+(i%3)*12,67,cx,cy);
  }
  const Rect top=branches[2];
  l2box(r,top.x,top.y-4,top.w,12,103,179,73,cx,cy);
  if(!collected)l2capy(r,2530,451,cx,cy);
  // Predio da clinica, telhado e porta de vidro.
  l2house(r,3340,1010,490,1490,244,226,197,cx,cy);
  l2box(r,3400,1220,134,270,83,151,166,cx,cy);
  l2box(r,3409,1228,116,258,174,221,228,cx,cy);
  l2box(r,3465,1228,6,258,88,142,158,cx,cy);
  l2box(r,3508,1358,11,13,244,229,186,cx,cy);
  l2box(r,3370,1070,431,55,79,158,169,cx,cy);
  l2label(r,"CONSULTORIO",3415,1086,4,255,248,232,cx,cy);
  // A auto-escola permanece ao lado; toldos para descer apertando A.
  l2house(r,3935,985,470,1490,235,205,173,cx,cy);
  l2box(r,3975,1040,408,55,92,128,113,cx,cy);
  l2label(r,"AUTO-ESCOLA PANTANAL",3987,1059,2,255,245,212,cx,cy);
  for(int i=2;i<5;i++){const Rect &p=roofs[i];l2box(r,p.x,p.y,p.w,p.h+10,96,160,170,cx,cy);l2box(r,p.x,p.y,p.w,6,203,234,231,cx,cy);}
  // Escadaria de pedra: solida, sobe ao andar para a esquerda.
  for(const auto&p:stairs){l2box(r,p.x,p.y,p.w,1650-p.y,158,154,140,cx,cy);l2box(r,p.x,p.y,p.w,10,210,208,188,cx,cy);}
  // Mamãe posicionada diante da porta, em escala maior que Laura.
  SDL_Texture* npcTex=nullptr;
  if(interaction==1)npcTex=mom[2];
  else if(interaction==2)npcTex=mom[3];
  else npcTex=mom[int(clock*1.25f)%2];
  const float mw=88,mh=147,mx=3480,feetMom=1250;
  SDL_Rect mother{int(mx-cx-mw/2),int(feetMom-cy-mh),int(mw),int(mh)};
  if(npcTex)SDL_RenderCopy(r,npcTex,nullptr,&mother);
  if(interaction==1||interaction==2){
   SDL_Texture* speech=mom[interaction==1?4:5];
   SDL_Rect bubble{int(mx-cx+38),int(feetMom-cy-mh-17),52,58};
   if(speech)SDL_RenderCopy(r,speech,nullptr,&bubble);
  }
  // Personagem Laura: preserva o padrao de desenho e animacao da Fase 1.
  int sel=0;
  if(!grounded){sel=vy<-90?8:vy<100?9:10;}
  else if(runClock>0)sel=1+int(runClock*1.2f)%6;
  constexpr int padding[18]={3,2,4,1,7,5,2,27,8,15,11,47,34,20,24,30,30,20};
  float sh=85,sw=sh*frames[sel]->w/frames[sel]->h;
  SDL_Rect player{int(x-cx-sw/2),int(feet-cy-sh+sh*padding[sel]/frames[sel]->h),int(sw),int(sh)};
  SDL_RenderCopyEx(r,laura[sel],nullptr,&player,0,nullptr,faceLeft?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);
  l2box(r,18,18,460,48,30,67,70,0,0);
  l2label(r,collected?"VA PARA O CONSULTORIO":"ACHE A CAPIVARA",32,32,3,245,234,198,0,0);
  if(collected)l2capy(r,520,68,0,0);
  if(!interaction&&!finished&&std::fabs(x-3525)<148&&std::fabs(feet-1250)<130){
   l2box(r,318,597,650,75,27,61,78,0,0);
   l2label(r,collected?"APERTE Y PARA FALAR":"PEGUE A CAPIVARA",345,615,4,255,233,188,0,0);
  }
  if(interaction){
   l2box(r,255,558,772,103,35,63,84,0,0);
   if(interaction==1)l2label(r,"MAMAE",490,590,6,255,235,193,0,0);
   else l2label(r,"MUITO BEM LAURA",335,591,5,255,234,197,0,0);
  }
  if(finished){
   l2box(r,255,190,770,330,30,83,80,0,0);
   l2box(r,269,204,742,303,66,143,124,0,0);
   l2label(r,"MISSAO CONCLUIDA",300,257,6,255,235,179,0,0);
   l2label(r,"MAMAE ESTA FELIZ",340,350,4,255,248,218,0,0);
   l2label(r,"APERTE X PARA REINICIAR",323,443,3,252,246,211,0,0);
  }
  if(fade>0){SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,0,0,0,Uint8(fade*255));SDL_Rect a{0,0,SW,SH};SDL_RenderFillRect(r,&a);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);}
 }
};
