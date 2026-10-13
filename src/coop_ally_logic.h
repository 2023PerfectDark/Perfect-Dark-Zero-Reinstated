#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace pdz_coop {
// Actor handles must include the native allocation generation, not just an
// address: a despawned enemy's permission must never pass to a new actor.
using Actor = uint64_t;
struct Position { float x{}, y{}, z{}; };
enum class Order { Hold, Follow, Defend };
struct Decision { Order order=Order::Hold; Actor target=0; };

enum class Partner { Player, Ally };
class RescuePolicy {
 public:
  bool Down(Partner who) {
    down_[unsigned(who)] = true;
    progress_ = 0;
    return !Wiped();
  }
  bool IsDown(Partner who) const { return down_[unsigned(who)]; }
  bool Wiped() const { return down_[0] && down_[1]; }
  bool Advance(Partner who, bool inReach, bool assisting, float seconds) {
    if (!IsDown(who) || Wiped() || !inReach || !assisting ||
        !std::isfinite(seconds) || seconds < 0) { progress_ = 0; return false; }
    progress_ += std::min(seconds, .3f);
    if (progress_ < 3.f) return false;
    down_[unsigned(who)] = false;
    progress_ = 0;
    return true;
  }
  void Reset() { down_ = {}; progress_ = 0; }
 private:
  std::array<bool, 2> down_{};
  float progress_ = 0;
};

class AllyPolicy {
 public:
  void Reset(Actor player, Actor ally) {
    player_=player; ally_=ally; threats_={}; following_=false;
  }
  void Forget(Actor actor) {
    for(auto& threat:threats_) if(threat.actor==actor) threat={};
    if(actor==player_ || actor==ally_) Reset(0,0);
  }
  // Called on confirmed enemy perception of Joanna. Merely hearing noise,
  // noticing the ally, or being close to Joanna does not authorize firing.
  void SpottedPlayer(Actor enemy, bool hostile) {
    if(!hostile || !ValidEnemy(enemy)) return;
    if(auto* threat=FindOrInsert(enemy)) threat->spotted=true;
  }
  // Fire event, not damage event: a shot aimed at Joanna may miss. Shots at
  // scenery, another NPC or the ally must not start this retaliation policy.
  void FiredAt(Actor enemy, Actor intendedTarget, bool hostile) {
    if(!hostile || intendedTarget!=player_ || !ValidEnemy(enemy)) return;
    if(auto* threat=Find(enemy); threat && threat->spotted) threat->fired=true;
  }
  bool CanEngage(Actor enemy) const {
    if(!ValidEnemy(enemy)) return false;
    for(const auto& threat:threats_)
      if(threat.actor==enemy) return threat.spotted && threat.fired;
    return false;
  }
  bool EnemyMayTargetAlly(Actor enemy) const {
    if(!ValidEnemy(enemy)) return false;
    for(const auto& threat:threats_)
      if(threat.actor==enemy) return threat.spotted;
    return false;
  }
  struct Candidate { Actor actor{}; Position position{}; bool alive{}, hostile{}, visible{}; };
  Decision Tick(Position player, Position ally, bool playerAlive, bool allyAlive,
                const Candidate* enemies, unsigned count) {
    if(!player_ || !ally_ || !playerAlive || !allyAlive || !Finite(player) || !Finite(ally)) {
      following_=false; return {};
    }
    // Distances are metres supplied by the native adapter. Hysteresis avoids
    // repeatedly starting/stopping at the follow distance. Never teleport.
    const float separation=DistanceSquared(player,ally);
    const float height=std::abs(player.y-ally.y);
    if(separation>25.f || height>.75f) following_=true;
    else if(separation<9.f && height<.45f) following_=false;
    Actor target=0; float closest=625.f;
    if(enemies) for(unsigned i=0;i<count;++i) {
      const auto& enemy=enemies[i];
      if(!enemy.alive || !enemy.hostile || !enemy.visible || !Finite(enemy.position) || !CanEngage(enemy.actor)) continue;
      const float distance=DistanceSquared(player,enemy.position);
      if(distance<closest) { closest=distance; target=enemy.actor; }
    }
    // Regroup when too far from Joanna; don't chase a retreating enemy.
    if(target && separation<=144.f) return {Order::Defend,target};
    return {following_?Order::Follow:Order::Hold,0};
  }
 private:
  struct Threat { Actor actor{}; bool spotted{}, fired{}; };
  std::array<Threat,128> threats_{};
  Actor player_=0, ally_=0;
  bool following_=false;
  bool ValidEnemy(Actor actor) const { return player_ && ally_ && actor && actor!=player_ && actor!=ally_; }
  Threat* Find(Actor actor) { for(auto& t:threats_) if(t.actor==actor) return &t; return nullptr; }
  Threat* FindOrInsert(Actor actor) {
    if(auto* t=Find(actor)) return t;
    for(auto& t:threats_) if(!t.actor) { t.actor=actor; return &t; }
    return nullptr; // Exhaustion holds fire; it never grants permission.
  }
  static bool Finite(Position p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
  static float DistanceSquared(Position a, Position b) {
    return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);
  }
};
}
