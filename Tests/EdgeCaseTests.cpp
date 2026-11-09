// Pac Shark simulation tests. No engine needed: Shim/ stands in for the few Unreal types the simulation uses.
// Build and run from this folder:
//   g++ -std=c++20 -O2 -IShim -I../Source/PacShark EdgeCaseTests.cpp -o edgetests && ./edgetests
//   cl /std:c++20 /EHsc /O2 /I Shim /I ..\Source\PacShark EdgeCaseTests.cpp && EdgeCaseTests.exe
// PacGame.cpp is compiled into this file (not passed separately), so the private-to-public trick below also
// applies to it; MSVC puts access levels in symbol names, so the two would not link otherwise.
#include "CoreMinimal.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <queue>
#include <set>
// White-box: private members are opened up so tests can set up exact situations. Standard headers come first.
#define _ALLOW_KEYWORD_MACROS
#define private public
#include "PacGame.h"
#include "PacGame.cpp"
#undef private
static int fails=0;
#define CHECK(c,...) do{ if(!(c)){ if(fails<30){printf("FAIL: " __VA_ARGS__); printf("\n");} ++fails; } }while(0)
const int W=28,H=31;
bool Open(const FPacGame&G,int x,int y){ auto t=G.GetTile(x,y); return t!=EPacTile::Wall && t!=EPacTile::Door; }
std::string Snapshot(const FPacGame&G){ std::string s; for(int y=0;y<H;++y)for(int x=0;x<W;++x){auto t=G.GetTile(x,y); s+= t==EPacTile::Wall?'#':'_';} return s; }
// Middle band from the template must survive unchanged.
const char* Mid[11]={
"######.##### ## #####.######","     #.##### ## #####.#     ","     #.##          ##.#     ","     #.## ###--### ##.#     ",
"######.## #      # ##.######","      .   #      #   .      ","######.## #      # ##.######","     #.## ######## ##.#     ",
"     #.##          ##.#     ","     #.## ######## ##.#     ","######.## ######## ##.######"};

void CheckMaze(const FPacGame&G,int power,const char*tag){
  int pw=0,pel=0;
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){
    auto t=G.GetTile(x,y);
    CHECK((t==EPacTile::Wall)==(G.GetTile(W-1-x,y)==EPacTile::Wall), "%s asymmetric at %d,%d",tag,x,y);
    if(t==EPacTile::PowerPellet)++pw; if(t==EPacTile::Pellet)++pel;
    if((y==0||y==H-1||x==0||x==W-1) && (y<9||y>19)) CHECK(t==EPacTile::Wall,"%s border open %d,%d",tag,x,y);
    if(y>=9&&y<=19){ char c=Mid[y-9][x]; CHECK((c=='#')==(t==EPacTile::Wall),"%s middle band changed %d,%d",tag,x,y); CHECK((c=='-')==(t==EPacTile::Door),"%s door moved",tag);}
  }
  CHECK(pw==power,"%s power %d expected %d",tag,pw,power);
  CHECK(pel>100,"%s only %d pellets",tag,pel);
  CHECK(Open(G,13,23)&&Open(G,14,23)&&G.GetTile(13,23)==EPacTile::Empty,"%s start blocked",tag);
  // no 2x2 open block anywhere outside the den
  for(int y=0;y<H-1;++y)for(int x=0;x<W-1;++x){
    if(y>=12&&y<=15&&x>=10&&x<=17) continue;
    if(y>=9&&y<=19&&(x<5||x>21)) continue; // outside void
    bool all=Open(G,x,y)&&Open(G,x+1,y)&&Open(G,x,y+1)&&Open(G,x+1,y+1);
    CHECK(!all,"%s 2x2 open at %d,%d",tag,x,y);
  }
  // reachability + no dead ends
  std::vector<int> seen(W*H,0); std::queue<std::pair<int,int>> q; q.push({13,23}); seen[23*W+13]=1;
  while(!q.empty()){ auto [x,y]=q.front(); q.pop(); int ex=0; int d[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
    for(auto&o:d){ int nx=((x+o[0])%W+W)%W, ny=y+o[1]; if(!Open(G,nx,ny))continue; ++ex; if(!seen[ny*W+nx]){seen[ny*W+nx]=1;q.push({nx,ny});}}
    CHECK(ex>=2,"%s dead end at %d,%d",tag,x,y); }
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){auto t=G.GetTile(x,y); if(t==EPacTile::Pellet||t==EPacTile::PowerPellet) CHECK(seen[y*W+x],"%s unreachable pellet %d,%d",tag,x,y);}
  // ghost den exit reachable
  CHECK(seen[11*W+13],"%s den exit unreachable",tag);
}

// Same math as APacSharkHUD::DeathFloatRise.
float DeathFloatRise(const FPacGame& Game, float Elapsed, float* OutCeiling=nullptr, float* OutHit=nullptr){
  const float T = Elapsed - 0.6f; const FVector2D Pos = Game.GetPacPos();
  const int32 X = FMath::RoundToInt(Pos.X); int32 Y = FMath::RoundToInt(Pos.Y);
  while (Y > 0 && Game.GetTile(X, Y - 1) != EPacTile::Wall && Game.GetTile(X, Y - 1) != EPacTile::Door) --Y;
  const float Ceiling = FMath::Max(0.f, (float)Pos.Y - (Y - 0.5f) - 0.45f);
  const float Speed=2.f; const float Accel=std::max(3.f,(Ceiling-Speed*1.5f)/(1.5f*1.5f));
  const float HitTime = (-Speed + std::sqrt(Speed*Speed + 4.f*Accel*Ceiling)) / (2.f*Accel);
  if(OutCeiling)*OutCeiling=Ceiling; if(OutHit)*OutHit=HitTime+0.6f;
  if (T <= 0.f) return 0.f;
  const float Rise = Speed*T + Accel*T*T; if (Rise < Ceiling) return Rise;
  const float SinceHit = T - HitTime; return std::max(0.f, Ceiling - 0.2f*std::exp(-5.f*SinceHit)*std::fabs(std::sin(SinceHit*9.f)));
}

// Bot: BFS to nearest pellet, optionally avoiding ghost tiles.
EPacDir BotDir(const FPacGame&G){
  FVector2D p=G.GetPacPos(); int sx=((FMath::RoundToInt(p.X))%W+W)%W, sy=FMath::RoundToInt(p.Y);
  std::vector<int> from(W*H,-1); std::queue<int> q; q.push(sy*W+sx); from[sy*W+sx]=sy*W+sx;
  int d[4][2]={{0,-1},{0,1},{-1,0},{1,0}}; EPacDir dn[4]={EPacDir::Up,EPacDir::Down,EPacDir::Left,EPacDir::Right};
  std::vector<int> firstDir(W*H,-1);
  while(!q.empty()){ int c=q.front(); q.pop(); int x=c%W,y=c/W;
    auto t=G.GetTile(x,y); if(c!=sy*W+sx && (t==EPacTile::Pellet||t==EPacTile::PowerPellet)) return dn[firstDir[c]];
    for(int k=0;k<4;++k){ int nx=((x+d[k][0])%W+W)%W, ny=y+d[k][1]; if(!Open(G,nx,ny))continue; int n=ny*W+nx; if(from[n]!=-1)continue; from[n]=c; firstDir[n]= c==sy*W+sx? k: firstDir[c]; q.push(n);} }
  return EPacDir::None;
}

int main(){
  // 1) maze generator over many seeds and every power count
  for(int s=0;s<3000;++s){ srand(s); FPacGame G; int p=s%9; G.SetPowerPelletCount(p); G.NewGame(); char tag[32]; snprintf(tag,32,"seed%d",s); CheckMaze(G,p,tag); }
  std::set<std::string> distinct; for(int s=0;s<200;++s){ srand(s); FPacGame G; G.NewGame(); distinct.insert(Snapshot(G)); }
  printf("1) maze checks done: %d distinct layouts in 200 games\n",(int)distinct.size());
  CHECK(distinct.size()>150,"too few distinct mazes");

  // 2) clamping
  { FPacGame G; G.SetGhostCount(0); G.SetPowerPelletCount(-3); G.NewGame(); CHECK(G.GetGhosts().Num()==1,"ghost clamp low"); CheckMaze(G,0,"clamp");
    G.SetGhostCount(99); G.SetPowerPelletCount(99); G.NewGame(); CHECK(G.GetGhosts().Num()==8,"ghost clamp high"); CheckMaze(G,8,"clamp8"); }
  printf("2) clamp checks done\n");

  // 3) gameplay sweeps
  float speeds[]={0.25f,0.4f,1.f,1.5f};
  int totalLevels=0,totalDeaths=0,frightWithZero=0,mazeChangedOnDeath=0,mazeSameOnLevel=0,countChangedMidLevel=0;
  for(int g=1;g<=8;++g) for(int p : {0,1,4,8}) for(float sp:speeds){
    srand(g*1000+p*10+(int)(sp*10)); FPacGame G; G.SetGhostCount(g); G.SetPowerPelletCount(p); G.SetGhostSpeedScale(sp); G.NewGame();
    std::string maze=Snapshot(G); int level=G.GetLevel();
    for(int f=0; f<60*60*6 && G.GetPhase()!=EPacGamePhase::GameOver; ++f){
      EPacDir d=BotDir(G); if(d!=EPacDir::None) G.QueueDirection(d);
      if(f==600) { G.SetGhostCount(9-g); G.SetPowerPelletCount(8-p); } // change settings mid-level
      auto before=G.GetPhase();
      float dt = (f%97==0)? 0.5f : 1.f/60.f;  // occasional lag spike
      G.Tick(dt);
      if(before!=EPacGamePhase::Dying && G.GetPhase()==EPacGamePhase::Dying) ++totalDeaths;
      if(G.GetFrightTimeLeft()>0 && p==0 && f<600) ++frightWithZero;
      if(G.GetLevel()!=level){ ++totalLevels; level=G.GetLevel(); std::string m=Snapshot(G); if(m==maze) ++mazeSameOnLevel; maze=m;
        CHECK(G.GetGhosts().Num()==9-g,"new level ghost count %d expected %d",G.GetGhosts().Num(),9-g); CheckMaze(G,8-p,"newlevel"); }
      else if(G.GetPhase()!=EPacGamePhase::GameOver){ std::string m=Snapshot(G); if(m!=maze) ++mazeChangedOnDeath; }
      if(level==1 && f>600 && G.GetGhosts().Num()!=g) ++countChangedMidLevel;
      FVector2D pp=G.GetPacPos(); CHECK(!G.IsWall(FMath::RoundToInt(pp.X),FMath::RoundToInt(pp.Y)),"pac in wall g%d p%d",g,p);
      for(int i=0;i<G.GetGhosts().Num();++i){ auto&gh=G.GetGhosts()[i]; if(gh.State==EPacGhostState::Active){ int gx=FMath::RoundToInt(gh.Pos.X),gy=FMath::RoundToInt(gh.Pos.Y);
        CHECK(!G.IsWall(gx,gy)&&G.GetTile(gx,gy)!=EPacTile::Door,"ghost %d in wall g%d sp%.2f at %.2f,%.2f",i,g,sp,gh.Pos.X,gh.Pos.Y); } }
    }
  }
  printf("3) gameplay: levels cleared %d, deaths %d\n",totalLevels,totalDeaths);
  CHECK(totalLevels>0,"bot never cleared a level");
  CHECK(frightWithZero==0,"fright happened with 0 power dots");
  CHECK(mazeChangedOnDeath==0,"maze changed without a new level (%d frames)",mazeChangedOnDeath);
  CHECK(mazeSameOnLevel==0,"maze repeated on new level");
  CHECK(countChangedMidLevel==0,"ghost count changed mid-level");

  // 4) ghosts all leave the den eventually with 8 ghosts at slowest speed
  { srand(7); FPacGame G; G.SetGhostCount(8); G.SetGhostSpeedScale(0.25f); G.NewGame(); int out=0;
    for(int f=0;f<60*40;++f){ G.Tick(1/60.f); if(G.GetPhase()==EPacGamePhase::Dying) break; }
    for(auto&gh:G.GetGhosts()){ out+= gh.State!=EPacGhostState::InHouse; if(gh.State==EPacGhostState::InHouse) CHECK(gh.ReleaseDelay>0.f,"ghost stuck in den"); } printf("4) 8 ghosts at 0.25 speed: %d out of the den after 40s or death\n",out); }

  // 5) death float: every open tile in many mazes
  float worstHit=0, worstCeil=0; int bad=0;
  for(int s=0;s<300;++s){ srand(s); FPacGame G; G.NewGame();
    for(int y=1;y<H-1;++y)for(int x=0;x<W;++x){ if(!Open(G,x,y)) continue; if(y>=12&&y<=15&&x>=10&&x<=17) continue; if(y>=9&&y<=19&&(x<5||x>22)) continue;
      float ceil,hit;
      int Y=y; while(Y>0 && G.GetTile(x,Y-1)!=EPacTile::Wall && G.GetTile(x,Y-1)!=EPacTile::Door) --Y;
      ceil = std::max(0.f,(float)y-(Y-0.5f)-0.45f); { float A=std::max(3.f,(ceil-3.f)/2.25f); hit=(-2.f+std::sqrt(4.f+4.f*A*ceil))/(2.f*A)+0.6f; }
      worstHit=std::max(worstHit,hit); worstCeil=std::max(worstCeil,ceil);
      if(hit>3.f-0.4f) ++bad;
      if(Y<1) ++bad; } }
  printf("5) death float: highest climb %.2f tiles, latest bump %.2fs (fade starts 2.6s)\n",worstCeil,worstHit);
  CHECK(bad==0,"%d tiles where the shark wouldn't reach the ceiling before fading",bad);
  // formula continuity & no overshoot on a real game
  { srand(3); FPacGame G; G.NewGame(); float c,h; float prev=0; for(float t=0;t<=3.f;t+=1/240.f){ float r=DeathFloatRise(G,t,&c,&h); CHECK(r<=c+1e-4,"overshoot"); CHECK(r>=-1e-4,"negative"); CHECK(std::fabs(r-prev)<0.2f,"jump at %.3f",t); prev=r;} }

  { int deathsChecked=0; for(int s=0;s<200 && deathsChecked<300;++s){ srand(s); FPacGame G; G.SetGhostCount(8); G.NewGame();
      for(int f=0;f<60*120 && G.GetPhase()!=EPacGamePhase::GameOver;++f){ if(rand()%15==0) G.QueueDirection((EPacDir)(1+rand()%4)); G.Tick(1/60.f);
        if(G.GetPhase()==EPacGamePhase::Dying && G.GetPhaseTimer()>FPacGame::DyingDuration-1/50.f){ float c,h; DeathFloatRise(G,0,&c,&h); ++deathsChecked;
          CHECK(h<=2.25f,"death at %.2f,%.2f hits ceiling at %.2fs",G.GetPacPos().X,G.GetPacPos().Y,h);
          float end=DeathFloatRise(G,2.6f); CHECK(std::fabs(end-c)<0.21f,"not resting at ceiling when fading (%.2f vs %.2f)",end,c);} } }
    printf("6) real deaths checked: %d\n",deathsChecked); }

  // 7) eaten eyes always get home: start eyes on every open tile, facing every way, in many mazes.
  { int stuck=0,total=0; float worst=0;
    for(int s=0;s<150;++s){ srand(s); FPacGame G; G.SetGhostCount(1); G.NewGame(); G.Phase=EPacGamePhase::Playing;
      for(int y=0;y<H;++y)for(int x=0;x<W;++x){ if(!Open(G,x,y)) continue; if(y>=12&&y<=15&&x>=10&&x<=17) continue; if(y>=9&&y<=19&&(x<5||x>22)) continue;
        for(int d=1;d<=4;++d){ FPacGhost& gh=G.Ghosts[0]; gh=FPacGhost(); gh.Pos=FVector2D(x,y); gh.State=EPacGhostState::Eaten; gh.Dir=(EPacDir)d;
          float t=0; while(t<15.f && gh.State==EPacGhostState::Eaten){ G.MoveGhost(0,1/60.f); t+=1/60.f; }
          ++total; if(gh.State==EPacGhostState::Eaten){ if(++stuck<4) printf("  eyes stuck: seed %d from %d,%d dir %d, now at %.2f,%.2f\n",s,x,y,d,gh.Pos.X,gh.Pos.Y);} else worst=std::max(worst,t); } } }
    printf("7) eaten eyes: %d of %d starts never got home, slowest %.2fs\n",stuck,total,worst);
    CHECK(stuck==0,"%d eyes circled without getting home",stuck); CHECK(worst<6.f,"eyes took %.2fs to get home",worst); }

  // 8) an octopus caught in the doorway while leaving the den (after a power dot) goes back in and comes out again.
  for(EPacDir d : {EPacDir::Up, EPacDir::Down}){ srand(1); FPacGame G; G.SetGhostCount(2); G.NewGame(); G.Phase=EPacGamePhase::Playing;
    G.Ghosts[0].Pos=FVector2D(1,1); FPacGhost& gh=G.Ghosts[1]; gh.State=EPacGhostState::Leaving; gh.Pos=FVector2D(13,11.4); gh.Dir=d; gh.bFrightened=true; G.FrightTimer=5;
    G.PacPos=FVector2D(13,11); int score=G.GetScore(); G.CheckCollisions();
    CHECK(G.GetScore()==score+200,"doorway catch scored %d",G.GetScore()-score);
    float t=0; while(t<10.f && gh.State!=EPacGhostState::Active){ G.MoveGhost(1,1/60.f); t+=1/60.f; }
    CHECK(gh.State==EPacGhostState::Active && !gh.bFrightened,"octopus caught in the doorway (dir %d) never came back out: state %d at %.2f,%.2f",(int)d,(int)gh.State,gh.Pos.X,gh.Pos.Y); }
  printf("8) doorway catch checked\n");

  // 9) bookkeeping during long random games: pellets, score, lives and ghost states always add up.
  { int games=0;
    for(int s=0;s<60;++s){ srand(500+s); FPacGame G; G.SetGhostCount(1+s%8); G.SetPowerPelletCount(s%9); G.SetGhostSpeedScale(0.25f+0.05f*(s%25)); G.NewGame(); ++games;
      int expectScore=0, extraLives=0, lastLevel=G.GetLevel();
      for(int f=0;f<60*180 && G.GetPhase()!=EPacGamePhase::GameOver;++f){
        EPacDir d = (s%3==0 && rand()%10==0) ? (EPacDir)(1+rand()%4) : BotDir(G); if(d!=EPacDir::None) G.QueueDirection(d);
        int combo=G.GhostCombo; G.Tick(f%61==0?0.25f:1/60.f);
        // Events come in order, so a power dot eaten earlier in the same step restarts the catch combo.
        for(EPacEvent e : G.TakeEvents()){ switch(e){ case EPacEvent::Pellet: expectScore+=10; break; case EPacEvent::PowerPellet: expectScore+=50; combo=0; break;
          case EPacEvent::GhostEaten: ++combo; expectScore+=100<<combo; break; case EPacEvent::ExtraLife: ++extraLives; break; default: break; } }
        CHECK(G.GetScore()==expectScore,"seed %d frame %d score %d expected %d",s,f,G.GetScore(),expectScore); expectScore=G.GetScore();
        int pellets=0; for(int y=0;y<H;++y)for(int x=0;x<W;++x){ auto t=G.GetTile(x,y); pellets+= t==EPacTile::Pellet||t==EPacTile::PowerPellet; }
        CHECK(pellets==G.GetPelletsLeft() || G.GetPhase()==EPacGamePhase::LevelClear,"seed %d pellets %d but counter says %d",s,pellets,G.GetPelletsLeft());
        CHECK(G.GetLives()>=0 && G.GetLives()<=4,"seed %d lives %d",s,G.GetLives());
        CHECK(G.GetLevel()==lastLevel || G.GetLevel()==lastLevel+1,"level jumped"); lastLevel=G.GetLevel();
        for(auto&gh:G.GetGhosts()){
          if(gh.State==EPacGhostState::Active) CHECK(gh.Dir!=EPacDir::None,"seed %d active octopus stopped at %.2f,%.2f",s,gh.Pos.X,gh.Pos.Y);
          if(gh.State==EPacGhostState::InHouse||gh.State==EPacGhostState::Leaving||gh.State==EPacGhostState::Entering)
            CHECK(gh.Pos.X>=10.99&&gh.Pos.X<=16.01&&gh.Pos.Y>=10.99&&gh.Pos.Y<=15.01,"seed %d octopus in state %d outside the den at %.2f,%.2f",s,(int)gh.State,gh.Pos.X,gh.Pos.Y);
          if(gh.State==EPacGhostState::Eaten||gh.State==EPacGhostState::Entering) CHECK(!gh.bFrightened,"eyes still frightened");
        }
        CHECK(G.GetFrightTimeLeft()>=0.f,"negative fright timer");
      }
      CHECK(extraLives<=1,"seed %d got %d extra lives",s,extraLives); }
    printf("9) bookkeeping checked over %d games\n",games); }

  // 10) odd frame times: zero, negative and huge steps never break anything.
  { srand(9); FPacGame G; G.NewGame(); float dts[]={0.f,-1.f,1e6f,1/60.f,-0.0001f,0.1f};
    for(int f=0;f<6000;++f){ G.QueueDirection((EPacDir)(f/40%5)); float pt=G.GetPhaseTimer(); auto ph=G.GetPhase(); G.Tick(dts[f%6]);
      if(dts[f%6]<=0.f && G.GetPhase()==ph) CHECK(G.GetPhaseTimer()==pt,"time ran backwards on a negative step");
      FVector2D p=G.GetPacPos(); CHECK(std::isfinite(p.X)&&std::isfinite(p.Y)&&!G.IsWall(FMath::RoundToInt(p.X),FMath::RoundToInt(p.Y)),"bad position after odd step"); }
    printf("10) odd frame times checked\n"); }

  // 11) replay checksum: a fixed set of bot games must play out exactly the same. If gameplay is changed on
  // purpose, run once and paste the new value here.
  { const uint64_t Expected=0x7f6f4b255fd9383full; uint64_t h=1469598103934665603ull; auto Mix=[&h](int64_t v){ h^=(uint64_t)v; h*=1099511628211ull; };
    for(int g=1;g<=8;++g) for(int p:{0,4,8}){ srand(g*7+p); FPacGame G; G.SetGhostCount(g); G.SetPowerPelletCount(p); G.SetGhostSpeedScale(0.5f+0.15f*g); G.NewGame();
      for(int f=0;f<60*90 && G.GetPhase()!=EPacGamePhase::GameOver;++f){ EPacDir d=BotDir(G); if(d!=EPacDir::None) G.QueueDirection(d); if(f%7==0 && rand()%5==0) G.QueueDirection((EPacDir)(1+rand()%4));
        G.Tick(f%113==0?0.2f:1/60.f);
        Mix((int64_t)(G.GetPacPos().X*1000)); Mix((int64_t)(G.GetPacPos().Y*1000)); Mix(G.GetScore()); Mix((int)G.GetPhase()); Mix(G.GetLives());
        for(auto&gh:G.GetGhosts()){ Mix((int64_t)(gh.Pos.X*1000)); Mix((int64_t)(gh.Pos.Y*1000)); Mix((int)gh.State); Mix(gh.bFrightened);}
        for(auto e:G.TakeEvents()) Mix((int)e); } }
    printf("11) replay checksum %016llx\n",(unsigned long long)h); CHECK(h==Expected,"replay checksum changed: gameplay is different"); }

  printf(fails? "\n%d FAILURES\n":"\nALL PASSED\n",fails); return fails?1:0;
}
