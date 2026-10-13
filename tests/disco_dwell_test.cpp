#include <initializer_list>
#include "../src/disco_dwell.h"
#include <cassert>
int main(){
  for(int fps:{30,60,180,360}){
    pdz_disco::Dwell d;
    for(int i=0;i<fps*3-1;++i)assert(!d.Update(true,1.0/fps));
    assert(d.Update(true,1.0/fps));assert(!d.Update(true,1.0/fps));
    d.Reset();for(int i=0;i<fps*2;++i)d.Update(true,1.0/fps);
    assert(!d.Update(false,1.0/fps));assert(d.seconds==0);
    for(int i=0;i<fps*2;++i)assert(!d.Update(true,1.0/fps));
    assert(!d.Update(true,1.0));assert(d.seconds==0);
  }
}

