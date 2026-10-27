#pragma once
#include <cmath>
namespace pdz_disco {
struct Dwell {
  double seconds=0;
  bool triggered=false;
  void Reset(){seconds=0;triggered=false;}
  bool Update(bool focused,double elapsed){
    if(triggered)return false;
    if(!focused||!std::isfinite(elapsed)||elapsed<0||elapsed>.25){seconds=0;return false;}
    seconds+=elapsed;
    if(seconds+1e-9<3.0)return false;
    triggered=true;return true;
  }
};
}
