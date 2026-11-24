#include "Game/Game.h"
#include "Game/GameConfig.h"
#include "Engine/Core/Profile.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Serialization/Serializer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
namespace {
using namespace Engine::Gameplay;
using EntityRef=ObjectRef<Entity>;
constexpr float Pi=3.14159265f;
constexpr std::array RiflePitch{Engine::AudioEnvelopePoint{0,1},Engine::AudioEnvelopePoint{.12f,.18f}};
constexpr std::array RifleVoices{Engine::ProceduralSoundVoice{.waveform=Engine::AudioWaveform::Noise,.frequencyHz=180,.volume=.5f,.envelope={.attackSeconds=.001f,.decaySeconds=.035f,.sustainLevel=.05f,.releaseSeconds=.03f},.frequencyMultiplier={.points=RiflePitch},.filter=Engine::AudioFilterType::LowPass,.filterCutoffHz=2600,.noiseSeed=0x7712u},Engine::ProceduralSoundVoice{.waveform=Engine::AudioWaveform::Square,.frequencyHz=95,.volume=.24f,.envelope={.attackSeconds=.001f,.decaySeconds=.025f,.sustainLevel=.1f,.releaseSeconds=.04f}}};
constexpr Engine::ProceduralSoundDefinition RifleSound{.durationSeconds=.12f,.voices=RifleVoices};
constexpr std::array ImpactVoices{Engine::ProceduralSoundVoice{.waveform=Engine::AudioWaveform::Noise,.frequencyHz=100,.volume=.45f,.envelope={.attackSeconds=0,.decaySeconds=.02f,.sustainLevel=.1f,.releaseSeconds=.06f},.filter=Engine::AudioFilterType::HighPass,.filterCutoffHz=700,.noiseSeed=0x914u}};
constexpr Engine::ProceduralSoundDefinition ImpactPatch{.durationSeconds=.08f,.voices=ImpactVoices};
float Length(Engine::Vector2 v){return std::sqrt(v.x*v.x+v.y*v.y);}
Engine::Vector2 Direction(float yaw){return {std::sin(yaw),std::cos(yaw)};}
}
ProceduralGame::ProceduralGame():world_(*gameInstance_.GetWorld()){RegisterTypes();EnsurePlayer();}
void ProceduralGame::RegisterTypes(){
 world_.RegisterComponent(Engine::Gameplay::MakeComponentType<FpsGame::PlayerController>("PlayerController"));
 world_.RegisterComponent(Engine::Gameplay::MakeComponentType<FpsGame::BotBrain>("BotBrain"));
 world_.RegisterComponent(Engine::Gameplay::MakeComponentType<FpsGame::BallisticFlight>("BallisticFlight"));
 world_.RegisterComponent(Engine::Gameplay::MakeComponentType<FpsGame::Vitality>("Vitality"));
 world_.RegisterEntity({FpsGame::PlayerType,"Player",{FpsGame::PlayerController::Type,FpsGame::Vitality::Type}});
 world_.RegisterEntity({FpsGame::BotType,"CombatBot",{FpsGame::BotBrain::Type,FpsGame::Vitality::Type}});
 world_.RegisterEntity({FpsGame::BulletType,"BallisticBullet",{FpsGame::BallisticFlight::Type}});
}
void ProceduralGame::EnsurePlayer(){for(auto id:world_.Entities()){EntityRef r(id);auto*e=r.Resolve();if(e&&e->GetTypeID()==FpsGame::PlayerType){player_=r;break;}}if(!player_.Resolve()){player_=world_.Spawn(FpsGame::PlayerType,"Player");world_.FlushSpawns();auto*e=player_.Resolve();e->transform.position={0,0};e->GetComponent<FpsGame::Vitality>().Resolve()->Configure(FpsGame::Faction::Player,100);}auto*e=player_.Resolve();controller_=e->GetComponent<FpsGame::PlayerController>();playerVitality_=e->GetComponent<FpsGame::Vitality>();}
void ProceduralGame::Initialize(){static_cast<void>(rifleSound_.Build(RifleSound));static_cast<void>(impactSound_.Build(ImpactPatch));static_cast<void>(rifleSound_.Upload());static_cast<void>(impactSound_.Upload());}
void ProceduralGame::Shutdown(){rifleSound_.Unload();impactSound_.Unload();}
void ProceduralGame::SpawnBot(){const auto* p=player_.Resolve();if(!p)return;const int count=static_cast<int>(world_.Entities().size());const float a=float(count*37%360)*Pi/180.0f;const float radius=19.0f+float(count%9);auto b=world_.Spawn(FpsGame::BotType,"Bot#"+std::to_string(count));world_.FlushSpawns();auto*e=b.Resolve();e->transform.position={p->transform.position.x+std::sin(a)*radius,p->transform.position.y+std::cos(a)*radius};e->GetComponent<FpsGame::BotBrain>().Resolve()->SetTarget(player_,static_cast<std::uint32_t>(count+0x9e3779b9u));e->GetComponent<FpsGame::Vitality>().Resolve()->Configure(FpsGame::Faction::Bot,4);}
void ProceduralGame::SpawnBullet(Engine::Vector2 origin,Engine::Vector2 direction,float height,float verticalSpeed,FpsGame::Faction faction){auto r=world_.Spawn(FpsGame::BulletType,"Bullet#"+std::to_string(gameInstance_.GetObjectManager()->LiveCount()));world_.FlushSpawns();auto*e=r.Resolve();e->transform.position=origin;e->GetComponent<FpsGame::BallisticFlight>().Resolve()->Configure(direction,height,verticalSpeed,faction);}
void ProceduralGame::Update(const Engine::InputSystem&input,float dt){ENGINE_PROFILE_SCOPE("BallisticBreach Update");auto cmd=inputBindings_.BuildPlayerCommand(input);if(auto*c=controller_.Resolve())c->Command(cmd.movement,cmd.yawDelta,cmd.pitchDelta,cmd.firing);world_.Update(std::min(dt,.05f));botSpawnTimer_-=dt;if(botSpawnTimer_<=0){std::size_t bots=0;for(auto id:world_.Entities()){auto*e=EntityRef(id).Resolve();bots+=e&&e->GetTypeID()==FpsGame::BotType;}if(bots<9)SpawnBot();botSpawnTimer_=2.1f;}
 if(auto*c=controller_.Resolve();c&&c->ConsumeShot()){float yaw=c->Yaw(),pitch=c->Pitch();Engine::Vector2 d=Direction(yaw)*std::cos(pitch);const auto*p=player_.Resolve();if(p)SpawnBullet(p->transform.position+d*0.7f,d*34.0f,1.62f,std::sin(pitch)*34.0f,FpsGame::Faction::Player);rifleSound_.Play(.8f,.94f, .5f);}
 ProcessWorld();}
void ProceduralGame::ProcessWorld(){std::vector<EntityRef> bullets;std::vector<EntityRef> dead;const auto ids=world_.Entities();for(auto id:ids){EntityRef r(id);auto*e=r.Resolve();if(!e)continue;if(e->GetTypeID()==FpsGame::BulletType)bullets.push_back(r);if(e->GetTypeID()==FpsGame::BotType){auto*b=e->GetComponent<FpsGame::BotBrain>().Resolve();Engine::Vector2 d;if(b&&b->ConsumeShot(d)){SpawnBullet(e->transform.position,d*19.0f,1.25f,0,FpsGame::Faction::Bot);rifleSound_.Play(.28f,.68f,.5f);}}}
 for(auto br:bullets){auto*b=br.Resolve();if(!b)continue;auto*f=b->GetComponent<FpsGame::BallisticFlight>().Resolve();if(!f)continue;if(f->Expired()){world_.Destroy(br);continue;}for(auto id:ids){EntityRef tr(id);auto*t=tr.Resolve();if(!t||t->GetTypeID()==FpsGame::BulletType)continue;auto*v=t->GetComponent<FpsGame::Vitality>().Resolve();if(!v||v->GetFaction()==f->GetFaction())continue;Engine::Vector2 delta{t->transform.position.x-b->transform.position.x,t->transform.position.y-b->transform.position.y};if(Length(delta)<.85f&&f->Height()>.15f&&f->Height()<2.2f){v->Damage(f->GetFaction()==FpsGame::Faction::Player?2:12);world_.Destroy(br);impactSound_.Play(.55f,.9f,.5f);if(v->Dead()){if(t->GetTypeID()==FpsGame::BotType)world_.Destroy(tr);else{v->Configure(FpsGame::Faction::Player,100);t->transform.position={0,0};}}break;}}}
 world_.FlushSpawns();}
Engine::Color ProceduralGame::GetClearColor()const{return GameConfig::BackgroundColor;}
void ProceduralGame::Render(Engine::RenderContext&context)const{const auto*p=player_.Resolve();const auto*c=controller_.Resolve();if(!p||!c)return;const float yaw=c->Yaw(),pitch=c->Pitch(),cp=std::cos(pitch);const Engine::Vector3 eye{p->transform.position.x,1.65f,p->transform.position.y};Engine::Camera3D camera;camera.position=eye;camera.target={eye.x+std::sin(yaw)*cp,eye.y+std::sin(pitch),eye.z+std::cos(yaw)*cp};camera.up={0,1,0};camera.fieldOfView=78;
 auto&r=context.Draw3D();r.Begin(camera);r.DrawGrid(80,1.0f);
 for(auto id:world_.Entities()){EntityRef ref(id);const auto*e=ref.Resolve();if(!e)continue;if(e->GetTypeID()==FpsGame::BotType){const auto*v=e->GetComponent<FpsGame::Vitality>().Resolve();const auto*b=e->GetComponent<FpsGame::BotBrain>().Resolve();const float hp=v?float(v->HP())/4:0;const float phase=b?b->AnimationPhase():0;const float gait=std::sin(phase)*.3f;const Engine::Color armor{static_cast<std::uint8_t>(210*(1-hp)+45),static_cast<std::uint8_t>(75+95*hp),105,255};const Engine::Color joint{205,210,193,255};Engine::Vector2 forward=b?b->Facing():Engine::Vector2{0,1};const float forwardLength=Length(forward);if(forwardLength>.001f){forward.x/=forwardLength;forward.y/=forwardLength;}else forward={0,1};const Engine::Vector2 right{forward.y,-forward.x};const float x=e->transform.position.x,z=e->transform.position.y;const Engine::Vector3 hips{x,0.83f,z},shoulder{x,1.43f,z};auto limb=[&](Engine::Vector3 a,Engine::Vector3 q,Engine::Color color){r.DrawLine(a,q,color);r.DrawSphere(a,.075f,color);r.DrawSphere(q,.075f,color);};r.DrawCube({x,1.12f,z},{.48f,.62f,.32f},armor);r.DrawSphere({x,1.62f,z},.22f,joint);r.DrawCubeOutline({x,1.12f,z},{.51f,.65f,.35f},{34,220,199,255});for(int side:{-1,1}){const float sign=float(side);Engine::Vector3 hip{hips.x+right.x*.12f*sign,hips.y,hips.z+right.y*.12f*sign};Engine::Vector3 knee{hip.x+forward.x*gait*sign, .43f, hip.z+forward.y*gait*sign};Engine::Vector3 foot{knee.x+forward.x*gait*sign,.08f,knee.z+forward.y*gait*sign};limb(hip,knee,armor);limb(knee,foot,joint);Engine::Vector3 upper{shoulder.x+right.x*.31f*sign,1.35f,shoulder.z+right.y*.31f*sign};Engine::Vector3 elbow{upper.x-forward.x*gait*sign,.98f,upper.z-forward.y*gait*sign};Engine::Vector3 hand{elbow.x-forward.x*gait*.5f*sign,.79f,elbow.z-forward.y*gait*.5f*sign};limb(upper,elbow,armor);limb(elbow,hand,joint);}}else if(e->GetTypeID()==FpsGame::BulletType){const auto*f=e->GetComponent<FpsGame::BallisticFlight>().Resolve();if(f)r.DrawSphere({e->transform.position.x,f->Height(),e->transform.position.y},.11f,f->GetFaction()==FpsGame::Faction::Player?Engine::Color{255,197,82,255}:Engine::Color{255,75,84,255});}}
 r.End();auto&ui=context.Draw2D();ui.DrawText("BALLISTIC // BREACH",{26,22},23,{117,244,220,255});ui.DrawText("WASD MOVE   MOUSE LOOK   LMB FIRE",{26,52},18,{225,232,236,255});auto*v=playerVitality_.Resolve();ui.DrawText("VITALS  "+std::to_string(v?v->HP():0)+"     HOSTILE UNITS ACTIVE",{26,82},18,{255,190,110,255});const float cx=context.GetWidth()*.5f,cy=context.GetHeight()*.5f;ui.DrawLine({cx-10,cy},{cx+10,cy},2,{120,255,225,230});ui.DrawLine({cx,cy-10},{cx,cy+10},2,{120,255,225,230});}
std::vector<std::byte> ProceduralGame::SaveResumeState()const{return world_.Save();}
void ProceduralGame::ResumeFromState(std::span<const std::byte>s){world_.Resume(s);player_={};controller_={};playerVitality_={};EnsurePlayer();}
#if defined(ENGINE_AUTOTESTS)
void ProceduralGame::SerializeAutoTestState(Engine::Serializer&s){int bots=0,bullets=0;for(auto id:world_.Entities()){EntityRef r(id);auto*e=r.Resolve();if(e){bots+=e->GetTypeID()==FpsGame::BotType;bullets+=e->GetTypeID()==FpsGame::BulletType;}}int hp=playerVitality_.Resolve()?playerVitality_.Resolve()->HP():0;s.Value("world.bots",bots);s.Value("world.ballisticBullets",bullets);s.Value("player.hitPoints",hp);}
#endif
