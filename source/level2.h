#pragma once
// Prototipo jogavel da Fase 2 - O Caminho das Nuvens.
// Usa o mesmo renderer, fisica 60 fps com dt e sprites da Laura da Fase 1.
// Todas as alturas das plataformas indicam a SUPERFICIE de aterrissagem.
#include "mamae_frames.h"

static constexpr float L2_W = 4290.f, L2_GROUND = 1490.f;
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
// Predios de condominio: varios pavimentos, paredes desgastadas e janelas.
static void l2house(SDL_Renderer*r,float x,float roof,float w,float bottom,int R,int G,int B,float cx,float cy){
 float top=roof+10;
 l2box(r,x,top,w,bottom-top,R,G,B,cx,cy);
 l2box(r,x-7,roof,w+14,11,137,76,69,cx,cy);
 l2box(r,x-7,roof+10,w+14,4,199,121,101,cx,cy);
 int cols=std::max(2,int(w/86)),floors=std::max(1,int((bottom-top-30)/135));
 for(int row=0;row<floors;++row)for(int col=0;col<cols;++col){
  float wx=x+22+col*(w-45)/cols, wy=top+43+row*130;
  l2box(r,wx-4,wy-4,48,62,153,132,116,cx,cy);
  l2box(r,wx,wy,40,53,75,126,137,cx,cy);
  l2box(r,wx+4,wy+4,32,45,147,203,208,cx,cy);
  l2box(r,wx+18,wy+4,3,45,88,143,153,cx,cy);
  l2box(r,wx+3,wy+27,34,3,84,137,151,cx,cy);
  l2box(r,wx-7,wy+56,54,5,171,131,111,cx,cy);
 }
 for(int row=0;row<int((bottom-top)/80);++row){
  float py=top+25+row*80;
  for(int col=0;col<4;++col){
   float xx=x+16+col*(w-34)/4+((row*17+col*11)%13);
   l2box(r,xx,py+((row+col)%3)*5,20+(col%3)*8,2+(row%2),
    std::max(0,R-28),std::max(0,G-24),std::max(0,B-17),cx,cy);
  }
 }
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
 // Coordenadas do mundo: Y diminui ao subir. "feet" e a base da Laura.
 static constexpr float pitLeft=690.f,pitRight=870.f;
 static constexpr float waterLeft=1509.f,waterRight=3130.f;
 static constexpr float treeTop=360.f, momX=4040.f,momFeet=1250.f;
 float x=385,feet=L2_GROUND,vy=0,camX=0,camY=845,clock=0,runClock=0,jumpElapsed=0,dropTimer=0,peakFeet=L2_GROUND;
 float fade=0,interactionTimer=0;
 int fadeState=0,interaction=0;
 bool grounded=true,faceLeft=false,collected=false,finished=false;
 std::vector<Rect> walls,roofs,branches,awnings,stairs,ground;
 std::vector<Cloud2> clouds;
 Level2(){
  ground={{0,L2_GROUND,pitLeft,310},{pitRight,L2_GROUND,waterLeft-pitRight,310},{waterRight,L2_GROUND,L2_W-waterRight,310}};
  // Retorno: caixa -> muros crescentes -> telhados das tres casas.
  // Muros sao de mao unica verticalmente, mas bloqueiam entrada lateral na subida.
  walls={{1175,1350,215,15},{970,1260,185,15},{785,1170,165,15},{710,1090,155,15}};
  // Condominio: primeiro predio quase da altura da tela e dois vizinhos distintos.
  roofs={{40,865,275,15},{350,965,255,15},{667,1055,245,15},
         {3150,535,350,15}, // Autoescola alta, proxima da copa da arvore
         {3670,965,450,15}}; // Consultorio mais baixo, ultimo predio
  // Galho opcional na arvore (nao e necessario para completar a rota).
  branches={{2538,480,240,15},{2550,treeTop,445,15}};
  auto cloud=[&](float xx,float yy,float ww,float amp=0,float speed=0,float phase=0){
    clouds.push_back({{xx,yy,ww,15},xx,amp,speed,phase,amp>0});
  };
  // Tres nuvens iniciais mantidas na mesma ordem, elevadas com o condominio.
  cloud(135,772,155);cloud(323,687,165);cloud(515,603,165);
  // Baixa um pouco, pega uma movel e atravessa a parte com agua.
  cloud(716,687,174);cloud(929,753,172,52,.9f,.7f);
  cloud(1128,752,168);cloud(1340,726,175,62,.9f,1.2f);
  cloud(1560,757,177);cloud(1785,735,185,64,1.0f,.5f);
  cloud(2020,670,184);cloud(2240,585,180,58,.84f,1.1f);
  cloud(2430,488,174);cloud(2560,450,170);
  // Saida da arvore por uma nuvem movel, sentido Autoescola.
  cloud(3038,407,174,58,.78f,.3f);
  // Tres nuvens empilhadas, para atravessar com A ate o telhado do dentista.
  cloud(3540,645,174);cloud(3572,758,174);cloud(3602,864,174);
  // Toldos empilhados na fachada da Autoescola, a ESQUERDA do consultorio.
  awnings={{3505,1075,195,13},{3505,1195,195,13},{3505,1315,195,13},{3505,1427,195,13}};
  // Escadaria na entrada do consultorio sobe da esquerda para direita.
  for(int i=0;i<5;++i)stairs.push_back({3745.f+i*65,1442.f-i*48,65.f,350.f+i*48});
 }
 void reset(){
  x=385;feet=L2_GROUND;vy=0;camX=0;camY=845;clock=0;runClock=0;
  jumpElapsed=0;dropTimer=0;peakFeet=L2_GROUND;fade=0;fadeState=0;
  interaction=0;interactionTimer=0;grounded=true;faceLeft=false;collected=false;finished=false;
  // Reposiciona nuvens moveis para uma nova tentativa.
  for(auto&c:clouds)c.r.x=c.origin+c.amplitude*std::sin(c.phase);
 }
 bool xhit(const Rect&p)const{return x+24>p.x&&x-24<p.x+p.w;}
 bool onPlatform(const Rect&p)const{return xhit(p)&&std::fabs(feet-p.y)<3.f;}
 bool validSupport()const{
  for(const auto&p:ground)if(onPlatform(p))return true;
  for(const auto&p:walls)if(onPlatform(p))return true;
  for(const auto&p:roofs)if(onPlatform(p))return true;
  for(const auto&p:branches)if(onPlatform(p))return true;
  for(const auto&p:awnings)if(onPlatform(p))return true;
  for(const auto&p:stairs)if(onPlatform(p))return true;
  if(onPlatform({1310,1426,64,64}))return true;
  for(const auto&p:clouds)if(onPlatform(p.r))return true;
  return false;
 }
 void die(SDL_AudioDeviceID audio,Mixer& mix,bool water){
  if(fadeState!=0)return;
  fadeState=1;fade=0;
  play(audio,mix,water?sfx_water:sfx_damaged1,water?sfx_water_count:sfx_damaged1_count,.75f);
 }
 void update(float dt,u64 down,float axis,SDL_AudioDeviceID audio,Mixer& mix){
  clock+=dt;
  if(fadeState){
   if(fadeState==1){fade=std::min(1.f,fade+dt/.38f);if(fade>=1.f){reset();fade=1;fadeState=2;}}
   else{fade=std::max(0.f,fade-dt/.46f);if(fade<=0.f)fadeState=0;}
   return;
  }
  if(interaction){
   interactionTimer+=dt;
   if(interaction==1&&interactionTimer>.75f){interaction=2;interactionTimer=0;}
   else if(interaction==2&&interactionTimer>1.4f){interaction=3;interactionTimer=0;}
   else if(interaction==3&&interactionTimer>.55f){
    interaction=0;interactionTimer=0;finished=true;
    if(audio){SDL_LockAudioDevice(audio);mix.musicPlaying=false;SDL_UnlockAudioDevice(audio);}
    play(audio,mix,sfx_complete,sfx_complete_count,.85f);
   }
   return;
  }
  if(finished)return;
  if((down&HidNpadButton_Y)&&collected&&std::fabs(x-momX)<116&&std::fabs(feet-momFeet)<85){
   interaction=1;interactionTimer=0;return;
  }
  if(axis<-.15f)faceLeft=true;else if(axis>.15f)faceLeft=false;
  if(std::fabs(axis)<.15f)axis=0;
  dropTimer=std::max(0.f,dropTimer-dt);
  // Se a Laura esta sobre uma nuvem movel, acompanha o movimento dela.
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
   for(const auto&p:awnings)passable|=onPlatform(p);
   for(const auto&c:clouds)passable|=onPlatform(c.r);
   if(passable){dropTimer=.22f;grounded=false;feet+=8;vy=160;
    play(audio,mix,laura_effort,laura_effort_count,.3f);}
  }
  if(grounded&&(down&HidNpadButton_B)){
   grounded=false;vy=-650;peakFeet=feet;jumpElapsed=0;
   play(audio,mix,laura_jump,laura_jump_count,.6f);
  }
  float oldX=x,oldFeet=feet;
  float nx=std::max(32.f,std::min(4086.f,x+axis*420*dt));
  // Caixa, grade e limite final com colisao lateral integral.
  auto side=[&](const Rect&p){
   if(feet<=p.y+2||feet-68>=p.y+p.h-2)return;
   if(nx>oldX&&oldX+24<=p.x+1&&nx+24>p.x)nx=p.x-24;
   if(nx<oldX&&oldX-24>=p.x+p.w-1&&nx-24<p.x+p.w)nx=p.x+p.w+24;
  };
  side({1310,1426,64,64});side({1475,1200,28,290});
  side({4110,0,65,1880});
  // Muros: entrada pela lateral bloqueada durante a escalada. Embaixo deles,
  // a rua continua livre; saltando por baixo e possivel atravessar a face superior.
  for(const auto&p:walls){
   if(feet>p.y+4&&feet<p.y+155&&feet<L2_GROUND-20){
    if(nx>oldX&&oldX+24<=p.x+1&&nx+24>p.x)nx=p.x-24;
    if(nx<oldX&&oldX-24>=p.x+p.w-1&&nx-24<p.x+p.w)nx=p.x+p.w+24;
   }
  }
  // Degraus sobem andando para a direita, sem precisar pular a cada degrau.
  for(const auto&p:stairs){
   if(nx>oldX&&oldX+24<=p.x+2&&nx+24>p.x&&grounded&&feet>=p.y&&feet-p.y<=53){
    feet=p.y;vy=0;grounded=true;
   }else side(p);
  }
  x=nx;
  if(!grounded){
   vy=std::min(1050.f,vy+1500.f*(vy>0?1.13f:1.f)*dt);
   feet+=vy*dt;jumpElapsed+=dt;peakFeet=std::min(peakFeet,feet);
  }
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
  for(const auto&p:awnings)land(p,true);
  for(const auto&c:clouds)land(c.r,true);
  if(landed){
   float fall=best-peakFeet;
   feet=best;vy=0;grounded=true;peakFeet=best;
   // Queda perigosa em terra firme fora dos pontos de apoio.
   if(fall>500&&best>=L2_GROUND-5){die(audio,mix,false);return;}
  }else if(grounded&&!validSupport()){
   grounded=false;vy=0;peakFeet=feet;
  }
  if(!collected&&grounded&&std::fabs(x-2740)<78&&std::fabs(feet-treeTop)<12){
   collected=true;play(audio,mix,sfx_healed1,sfx_healed1_count,.8f);
  }
  bool inPit=x>pitLeft+8&&x<pitRight-8;
  bool inWater=x>waterLeft&&x<waterRight;
  // No fosso, a Laura desaparece ate metade abaixo da imagem antes do fade.
  // Enquanto cai no fosso, a camera vertical nao segue a personagem.
  if(inPit&&feet-camY>SH+42){die(audio,mix,false);return;}
  if(inWater&&feet>=L2_GROUND+24){die(audio,mix,true);return;}
  if(feet>1810){die(audio,mix,false);return;}
  if(grounded&&std::fabs(axis)>.15f)runClock+=dt*11;else runClock=0;
  camX+=(std::max(0.f,std::min(L2_W-SW,x-420))-camX)*std::min(1.f,dt*5.5f);
  if(!inPit&&!(inWater&&feet>L2_GROUND)){
   // Na descida mostra mais area INFERIOR; na subida revela nuvens superiores.
   bool descendingClouds=(x>2990&&feet<1450)||(vy>110&&feet<1200);
   float desired=feet-(descendingClouds?278.f:(feet>1190&&x<1510?645.f:510.f));
   camY+=(std::max(0.f,std::min(1120.f,desired))-camY)*std::min(1.f,dt*4.2f);
  }
 }
 void render(SDL_Renderer*r,SDL_Texture* mom[6],SDL_Texture* laura[18],const FrameData* frames[18])const{
  float cx=camX,cy=camY;
  SDL_SetRenderDrawColor(r,122,218,232,255);SDL_RenderClear(r);
  // Montanhas e construcoes distantes em paralaxe suave.
  for(int i=-1;i<17;++i){
   float px=i*440-cx*.16f;
   l2ellipse(r,px+150,870-cy*.08f,260,195,130,224,209,0,0);
   l2ellipse(r,px+240,1010-cy*.07f,285,175,100,205,183,0,0);
   l2ellipse(r,px+70,1160-cy*.04f,250,215,64,191,148,0,0);
  }
  for(int i=0;i<23;++i){
   float bx=i*225+50-cx*.26f, by=1150-cy*.18f+(i%3)*37;
   l2box(r,bx,by,105,320,221,200,165,0,0);
   l2box(r,bx-6,by-18,117,24,168,102,83,0,0);
   for(int k=0;k<3;k++)l2box(r,bx+22+(k%2)*44,by+33+k*60,27,37,110,176,187,0,0);
  }
  // Solo da vila e margens do rio.
  for(const auto&p:ground){
   l2box(r,p.x,p.y,p.w,340,116,86,61,cx,cy);
   l2box(r,p.x,p.y,p.w,15,87,162,77,cx,cy);
   l2box(r,p.x,p.y-8,p.w,11,123,207,93,cx,cy);
   for(int i=0;i<int(p.w)/46;i++)l2box(r,p.x+i*46+9,p.y+42+(i%3)*31,14,9,91,65,52,cx,cy);
  }
  // Fosso verdadeiro escuro; a abertura permanece vazia, sem agua azul.
  l2box(r,pitLeft,L2_GROUND,pitRight-pitLeft,360,30,20,19,cx,cy);
  l2box(r,pitLeft+12,L2_GROUND+12,pitRight-pitLeft-24,350,14,11,14,cx,cy);
  l2box(r,pitLeft,L2_GROUND,9,340,67,47,38,cx,cy);
  l2box(r,pitRight-9,L2_GROUND,9,340,67,47,38,cx,cy);
  // Agua depois da grade: uma queda aqui reinicia a fase.
  l2box(r,waterLeft,L2_GROUND,waterRight-waterLeft,360,38,125,157,cx,cy);
  for(int i=0;i<38;i++){
   float xx=waterLeft+10+i*43,wob=5*std::sin(clock*2+i*.8f);
   l2box(r,xx,L2_GROUND+12+wob,25,3,158,232,229,cx,cy);
  }
  // Tres edificios diferentes no condominio. O primeiro fica quase a altura da tela.
  l2house(r,40,865,275,L2_GROUND,205,182,160,cx,cy);
  l2house(r,350,965,255,L2_GROUND,222,168,143,cx,cy);
  l2house(r,667,1055,245,L2_GROUND,204,199,170,cx,cy);
  l2label(r,"CONDOMINIO",75,940,2,103,81,74,cx,cy);
  // Uma fachada frontal continua faz os muros parecerem parte da rua, nao escadinhas soltas.
  l2box(r,713,1135,760,L2_GROUND-1135,174,155,137,cx,cy);
  for(int row=0;row<6;++row)for(int col=0;col<11;++col){
   float px=722+col*67+(row%2)*20,py=1170+row*51;
   if(px<1460)l2box(r,px,py,25+(col%3)*7,3,137,115,98,cx,cy);
  }
  for(const auto&p:walls){
   l2box(r,p.x,p.y,p.w,1500-p.y,171,151,123,cx,cy);
   l2box(r,p.x,p.y-7,p.w,13,208,185,149,cx,cy);
   for(int i=0;i<int(p.w)/48;i++)l2box(r,p.x+9+i*48,p.y+28+(i%2)*22,27,4,131,113,92,cx,cy);
  }
  l2box(r,1310,1426,64,64,103,66,43,cx,cy);
  l2box(r,1315,1431,54,54,178,120,74,cx,cy);
  l2box(r,1339,1431,5,54,111,77,50,cx,cy);
  for(int i=0;i<7;i++)l2box(r,1475+i*4,1200,2,290,86,95,101,cx,cy);
  l2box(r,1471,1200,36,13,78,88,95,cx,cy);
  l2box(r,1471,1330,36,9,78,88,95,cx,cy);
  // Nuvenzinhas brancas do percurso, sem plataformas artificiais no cenario.
  for(const auto&cl:clouds)l2cloud(r,cl.r,cx,cy,cl.moving);
  // Arvore gigante, copa segura e capivarinha de pelucia.
  l2box(r,2745,475,110,1015,131,99,67,cx,cy);
  l2box(r,2764,550,18,900,164,121,78,cx,cy);
  l2box(r,2645,480,176,13,134,92,56,cx,cy);
  l2box(r,2645,472,176,8,172,126,82,cx,cy);
  for(int i=0;i<9;i++){
   float tx=2560+i*54;
   l2ellipse(r,tx,346+(i%3)*10,110,77,59+(i%3)*8,156+(i%3)*10,76,cx,cy);
  }
  l2box(r,2550,treeTop-3,445,13,109,183,79,cx,cy);
  if(!collected)l2capy(r,2740,treeTop-4,cx,cy);
  // Autoescola mais alta e a ESQUERDA do consultorio.
  l2house(r,3150,535,350,L2_GROUND,227,198,158,cx,cy);
  l2box(r,3175,625,292,55,102,138,122,cx,cy);
  l2label(r,"AUTO-ESCOLA",3190,642,2,255,245,212,cx,cy);
  l2label(r,"PANTANAL",3215,670,2,255,245,212,cx,cy);
  for(const auto&p:awnings){
   l2box(r,p.x,p.y,p.w,18,94,151,163,cx,cy);
   l2box(r,p.x,p.y,p.w,7,205,233,230,cx,cy);
   for(int k=0;k<7;++k)l2box(r,p.x+k*27,p.y+7,13,9,77,132,145,cx,cy);
  }
  // Consultorio e o ULTIMO predio; altura menor que a Autoescola.
  l2house(r,3670,965,450,L2_GROUND,246,228,205,cx,cy);
  l2box(r,3700,1010,388,59,81,158,169,cx,cy);
  l2label(r,"CONSULTORIO",3717,1030,3,255,248,232,cx,cy);
  // Degraus na direcao correta: da ESQUERDA (baixo) para DIREITA (alto).
  for(const auto&p:stairs){
   l2box(r,p.x,p.y,p.w,L2_GROUND-p.y+12,157,150,138,cx,cy);
   l2box(r,p.x,p.y,p.w,10,213,209,192,cx,cy);
  }
  // Porta de vidro na altura do patamar onde a Mamae espera olhando para a esquerda.
  l2box(r,4020,1103,100,147,83,151,166,cx,cy);
  l2box(r,4027,1110,86,140,174,221,228,cx,cy);
  l2box(r,4066,1110,6,140,88,142,158,cx,cy);
  l2box(r,4097,1190,9,12,244,229,186,cx,cy);
  // Limite DIREITO invisivel do jogo: nao ha estrada depois do dentista.
  SDL_Texture* npcTex=nullptr;
  if(interaction==1)npcTex=mom[2];
  else if(interaction==2)npcTex=mom[3];
  else npcTex=mom[int(clock*1.25f)%2];
  const float mw=88,mh=147;
  SDL_Rect mother{int(momX-cx-mw/2),int(momFeet-cy-mh),int(mw),int(mh)};
  if(npcTex)SDL_RenderCopyEx(r,npcTex,nullptr,&mother,0,nullptr,SDL_FLIP_HORIZONTAL);
  if(interaction==1||interaction==2){
   SDL_Texture* speech=mom[interaction==1?4:5];
   SDL_Rect bubble{int(momX-cx+37),int(momFeet-cy-mh-15),52,58};
   if(speech)SDL_RenderCopy(r,speech,nullptr,&bubble);
  }
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
  if(!interaction&&!finished&&std::fabs(x-momX)<148&&std::fabs(feet-momFeet)<125){
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
  if(fade>0){
   SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(r,0,0,0,Uint8(fade*255));
   SDL_Rect a{0,0,SW,SH};SDL_RenderFillRect(r,&a);
   SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
  }
 }
};
