#pragma once
#include "Engine/Gameplay/Object.h"
#include "Engine/Math/Vector2.h"
#include <cstdint>
namespace FpsGame {
using Engine::Gameplay::ObjectRef; using Engine::Gameplay::StateReader; using Engine::Gameplay::StateWriter; using Engine::Gameplay::TypeID;
enum class Faction : std::uint8_t { Player, Bot };
inline constexpr TypeID PlayerType=Engine::Gameplay::StableTypeID("BallisticBreach.PlayerEntity.v1");
inline constexpr TypeID BotType=Engine::Gameplay::StableTypeID("BallisticBreach.BotEntity.v1");
inline constexpr TypeID BulletType=Engine::Gameplay::StableTypeID("BallisticBreach.BallisticBulletEntity.v1");
class PlayerController final: public Engine::Gameplay::Component {
public: static constexpr TypeID Type=Engine::Gameplay::StableTypeID("BallisticBreach.PlayerController.v1"); explicit PlayerController(ObjectRef<Engine::Gameplay::Entity> o):Component(o){} TypeID GetTypeID()const override{return Type;} std::uint32_t CurrentStateVersion()const override{return 1;} std::uint32_t MinimumStateVersion()const override{return 1;} void Command(Engine::Vector2 m,float yd,float pd,bool fire){move_=m;yawDelta_=yd;pitchDelta_=pd;fire_=fire;} float Yaw()const{return yaw_;} float Pitch()const{return pitch_;} bool ConsumeShot(); void Update(float dt)override; void SaveState(StateWriter&)const override; void LoadState(StateReader&,std::uint32_t)override;
private: Engine::Vector2 move_{}; float yaw_=0,pitch_=0,yawDelta_=0,pitchDelta_=0,cooldown_=0; bool fire_=false,shot_=false;
};
class BotBrain final: public Engine::Gameplay::Component {
public: static constexpr TypeID Type=Engine::Gameplay::StableTypeID("BallisticBreach.BotBrain.v1"); explicit BotBrain(ObjectRef<Engine::Gameplay::Entity> o):Component(o){} TypeID GetTypeID()const override{return Type;} std::uint32_t CurrentStateVersion()const override{return 4;} std::uint32_t MinimumStateVersion()const override{return 1;} void SetTarget(ObjectRef<Engine::Gameplay::Entity> t,std::uint32_t seed=1){target_=t;rngState_=seed?seed:1;waypointTimer_=0;} bool ConsumeShot(Engine::Vector2& d); float AnimationPhase()const{return animationPhase_;} Engine::Vector2 Facing()const{return facing_;} void Update(float dt)override; void SaveState(StateWriter&)const override; void LoadState(StateReader&,std::uint32_t)override;
private: ObjectRef<Engine::Gameplay::Entity> target_; float cooldown_=1.0f; Engine::Vector2 shotDirection_{},waypoint_{},facing_{0,1}; float animationPhase_=0,waypointTimer_=0,shootPause_=0; std::uint32_t rngState_=1; bool shot_=false;
};
class Vitality final: public Engine::Gameplay::Component {
public: static constexpr TypeID Type=Engine::Gameplay::StableTypeID("BallisticBreach.Vitality.v1"); explicit Vitality(ObjectRef<Engine::Gameplay::Entity> o):Component(o){} TypeID GetTypeID()const override{return Type;} std::uint32_t CurrentStateVersion()const override{return 1;} std::uint32_t MinimumStateVersion()const override{return 1;} void Configure(Faction f,int hp){faction_=f;hp_=hp;} void Damage(int n){hp_-=n;} int HP()const{return hp_;} bool Dead()const{return hp_<=0;} Faction GetFaction()const{return faction_;} void SaveState(StateWriter&)const override; void LoadState(StateReader&,std::uint32_t)override;
private: Faction faction_=Faction::Bot; int hp_=1;
};
class BallisticFlight final: public Engine::Gameplay::Component {
public: static constexpr TypeID Type=Engine::Gameplay::StableTypeID("BallisticBreach.BallisticFlight.v1"); explicit BallisticFlight(ObjectRef<Engine::Gameplay::Entity> o):Component(o){} TypeID GetTypeID()const override{return Type;} std::uint32_t CurrentStateVersion()const override{return 1;} std::uint32_t MinimumStateVersion()const override{return 1;} void Configure(Engine::Vector2 v,float h,float vy,Faction f){velocity_=v;height_=h;verticalSpeed_=vy;faction_=f;} float Height()const{return height_;} Faction GetFaction()const{return faction_;} bool Expired()const{return lifetime_<=0||height_<0;} void Update(float dt)override; void SaveState(StateWriter&)const override; void LoadState(StateReader&,std::uint32_t)override;
private: Engine::Vector2 velocity_{}; float height_=1,verticalSpeed_=0,lifetime_=4; Faction faction_=Faction::Player;
};
}
