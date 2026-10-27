#pragma once
#include <algorithm>
#include <cmath>
#include <atomic>
namespace breathing_motion {
inline std::atomic<float> visiblePhase{0}, visibleExertion{0}, damageImpulse{0};
inline bool SlowRecoveryWeapon(unsigned id) {
  return id!=0&&id!=2&&id!=7&&id!=11&&id!=15&&id!=16;
}
struct Sample {float pitch=0,roll=0;};
struct State {
  double phase=0;float stepAge=1,exertion=0,roll=0,rollVelocity=0;
  Sample Step(float dt,float speed,bool enabled,bool zoomed,bool slowRecovery=false,float forward=0,float sideways=0,bool footstep=false,bool publish=true) {
    if(!enabled){phase=0;stepAge=1;exertion=0;roll=0;rollVelocity=0;return {};}
    if(!std::isfinite(dt)||dt<=0||dt>.1f)return {};
    const float effort=std::isfinite(speed)?std::clamp(speed/400.f,0.f,1.f):0.f;
    exertion+=(effort-exertion)*(1.f-std::exp(-dt*(effort>exertion?1.2f:(slowRecovery?.09f:.22f))));
    exertion=std::clamp(exertion+damageImpulse.exchange(0.f),0.f,1.f);
    phase=std::fmod(phase+dt/(8.f-6.f*exertion)*6.283185307,6.283185307);
    if(publish){visiblePhase.store(float(phase));visibleExertion.store(exertion);}
    const float gain=1.6f*(zoomed?.22f:1.f);
    const bool moving=std::isfinite(speed)&&speed>5.f;
    forward=std::isfinite(forward)?std::clamp(forward,-1.f,1.f):0;
    sideways=std::isfinite(sideways)?std::clamp(sideways,-1.f,1.f):0;
    // Native footstep audio is the sole gait clock. No distance oscillator.
    stepAge=footstep?0.f:std::min(1.f,stepAge+dt);
    const float bob=stepAge<.24f?-.004f*std::sin(3.14159265f*stepAge/.24f)*std::exp(-stepAge*5.f):0.f;
    const float targetRoll=gain*(moving?sideways*(effort*.003f+bob*.3f):
              (.002f+.006f*exertion)*float(std::sin(phase+.7)));
    // Ease angle AND angular velocity through idle/strafe/zoom transitions.
    // The exact critically damped spring is stable across render rates.
    // Keep the footstep pitch impulse outside this filter to retain sound sync.
    constexpr float rate=12.f;
    const float displacement=roll-targetRoll;
    const float impulse=rollVelocity+rate*displacement;
    const float decay=std::exp(-rate*dt);
    roll=targetRoll+(displacement+impulse*dt)*decay;
    rollVelocity=(rollVelocity-rate*impulse*dt)*decay;
    return {gain*((.0012f+.002f*exertion)*float(std::sin(phase))+bob*forward),roll};
  }
};
}
