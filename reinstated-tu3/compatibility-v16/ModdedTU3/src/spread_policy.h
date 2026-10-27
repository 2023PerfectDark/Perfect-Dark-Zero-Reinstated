#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace pdz_spread {
struct State {
  uint64_t key=0;
  int style=0;
  double last=0, bloom=0;
  double Apply(uint64_t identity,int mode,double cone,double nativeBloom,double zoom,double now) {
    if(mode==0) { *this={}; return double(float(cone+nativeBloom)); }
    if(!std::isfinite(cone)||!std::isfinite(nativeBloom)) return double(float(cone+nativeBloom));
    cone=std::max(0.,cone);
    if(key!=identity||style!=mode||now<last) {key=identity;style=mode;last=now;bloom=0;}
    double elapsed=std::max(0.,now-last); last=now;
    if(mode==2) {bloom=0;return cone;}
    // Calls sharing a timestamp (shotgun pellets) cannot accumulate extra bloom.
    // Long gaps recover before the next shot, rather than counting as firing.
    if(elapsed>.25) bloom*=std::exp(-(elapsed-.25)*4.);
    double dt=std::min(elapsed,.15);
    zoom=std::isfinite(zoom)?std::clamp(zoom,1.,20.):1.;
    double aim=std::clamp((zoom-1.)/.25,0.,1.);
    double target=std::clamp(nativeBloom,0.,cone*1.5)*(1.-aim);
    double rate=target>bloom?0.65:5.;
    bloom+=(target-bloom)*(1.-std::exp(-rate*dt));
    return (cone+bloom)/std::pow(zoom,1.5);
  }
};
}
