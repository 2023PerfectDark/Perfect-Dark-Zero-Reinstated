#pragma once
#include "utils.h"
#include <imgui.h>
#include <fstream>
#include <vector>
#include <string_view>
#include <algorithm>
#include <cmath>

namespace pdz_menu {
// Original NTSC font format from perfect_dark types.h/fontchar and text.c.
// Assets are extracted locally, never embedded in the executable.
class BitmapFont {
  std::vector<unsigned char> data_;
  inline static int atlas_rect_=-1;
  // 12x covers the 9x scale of a 2160px display with room for filtering.
  static constexpr int raster_scale_=12;
  int Word(size_t at) const {
    return int((uint32_t(data_[at])<<24)|(uint32_t(data_[at+1])<<16)|
               (uint32_t(data_[at+2])<<8)|data_[at+3]);
  }
 public:
  explicit BitmapFont(const char* name) {
    std::ifstream in(utils::RepoRoot()/"mod_assets"/"pd-menu"/name,std::ios::binary);
    data_.assign(std::istreambuf_iterator<char>(in),{});
    if(data_.size()<1804)data_.clear();
    for(int i=0;!data_.empty()&&i<94;++i) {
      size_t p=676+i*12;
      if(Word(p+4)<0||Word(p+4)>=13||Word(p+8)<0||
         size_t(Word(p+8))+(data_[p+2]+2)*8>data_.size()||data_[p+3]>14)data_.clear();
    }
  }
  bool Ready() const{return !data_.empty();}
  static void PrepareAtlas(ImFontAtlas* atlas) {
    BitmapFont original("handelgothicsm.bin");
    if(!original.Ready())return;
    // Reconstruct the bitmap's coverage contour, then rasterize its edge at
    // high resolution. Simply enlarging the alpha tile retains a several-
    // pixel blur at 4K. Original glyph metrics and kerning stay unchanged.
    atlas_rect_=atlas->AddCustomRectRegular(256*raster_scale_,128*raster_scale_);
    atlas->Build();
    unsigned char* pixels;int width,height;
    atlas->GetTexDataAsRGBA32(&pixels,&width,&height);
    const auto* rect=atlas->GetCustomRectByIndex(atlas_rect_);
    // Optional local XBLA artwork, fitted to the same cells and metrics.
    // Keep the original font as a fallback for installations without it.
    std::ifstream hd(utils::RepoRoot()/"mod_assets"/"pd-menu"/"xbla-handel-menu.rgba",std::ios::binary|std::ios::ate);
    constexpr size_t hd_width=256*raster_scale_,hd_height=128*raster_scale_;
    if(hd && hd.tellg()==std::streamoff(hd_width*hd_height*4)) {
      std::vector<unsigned char> rgba(hd_width*hd_height*4);
      hd.seekg(0);
      if(hd.read(reinterpret_cast<char*>(rgba.data()),rgba.size())) {
        for(size_t y=0;y<hd_height;++y)
          std::copy_n(rgba.data()+y*hd_width*4,hd_width*4,
                      pixels+((rect->y+y)*width+rect->x)*4);
        return;
      }
    }
    for(int i=0;i<94;++i){size_t p=676+i*12;int offset=original.Word(p+8);
      auto coverage=[&](int x,int y){
        if(x<0||y<0||x>=16||y>=original.data_[p+2]+2||x>=original.data_[p+3]+2)return 0.f;
        unsigned v=original.data_[offset+y*8+x/2];v=(x&1)?v&15:v>>4;
        return v<9?0.f:v==15?1.f:float((v-8)*36)/255.f;
      };
      for(int y=0;y<16*raster_scale_;++y)for(int x=0;x<16*raster_scale_;++x){
        float sx=(x+.5f)/raster_scale_-.5f,sy=(y+.5f)/raster_scale_-.5f;
        int ix=int(std::floor(sx)),iy=int(std::floor(sy));float fx=sx-ix,fy=sy-iy;
        float top=coverage(ix,iy)*(1-fx)+coverage(ix+1,iy)*fx;
        float bottom=coverage(ix,iy+1)*(1-fx)+coverage(ix+1,iy+1)*fx;
        float value=top*(1-fy)+bottom*fy;
        int a=int(255.f*std::clamp((value-.5f)*raster_scale_+ .5f,0.f,1.f)+.5f);
        size_t dest=((rect->y+(i/16)*16*raster_scale_+y)*width+rect->x+(i%16)*16*raster_scale_+x)*4;
        pixels[dest]=pixels[dest+1]=pixels[dest+2]=255;pixels[dest+3]=a;
      }
    }
  }
  float Text(ImDrawList* draw,ImVec2 pos,float scale,ImU32 colour,std::string_view text) const {
    if(!Ready())return 0;
    float x=pos.x, longest=0; int prev='H';
    for(unsigned char c:text) {
      if(c=='\n'){longest=std::max(longest,x-pos.x);x=pos.x;pos.y+=13*scale;prev='H';continue;}
      if(c==' '){x+=5*scale;prev='H';continue;}
      if(c<33||c>126)c='?';
      size_t p=676+(c-33)*12, q=676+(prev-33)*12;
      x-=(Word((Word(q+4)*13+Word(p+4))*4)-1)*scale;
      const int h=data_[p+2],w=data_[p+3],base=int(int8_t(data_[p+1]));
      const size_t pixels=Word(p+8);
      if(draw&&atlas_rect_>=0){
        auto* atlas=ImGui::GetIO().Fonts;const auto* rect=atlas->GetCustomRectByIndex(atlas_rect_);
        const float u=float(rect->x+(((c-33)%16)*16+1)*raster_scale_),v=float(rect->y+(((c-33)/16)*16+1)*raster_scale_);
        draw->AddImage(atlas->TexID,{x,pos.y+base*scale},{x+w*scale,pos.y+(base+h)*scale},
          {u*atlas->TexUvScale.x,v*atlas->TexUvScale.y},{(u+w*raster_scale_)*atlas->TexUvScale.x,(v+h*raster_scale_)*atlas->TexUvScale.y},colour);
      }else if(draw)for(int y=0;y<h;++y)for(int col=0;col<w;) {
        // text_render_v2 samples at (1,1): the stored CI4 tile includes a
        // one-pixel outline on every side, outside the glyph dimensions.
        auto alpha=[&](int cx){++cx;unsigned v=data_[pixels+(y+1)*8+cx/2];v=(cx&1)?v&15:v>>4;return v<9?0:v==15?255:(v-8)*36;};
        int a=alpha(col),end=col+1;while(end<w&&alpha(end)==a)++end;
        if(a){ImU32 tint=(colour&0xffffff)|(((colour>>24)*a/255)<<24);
          draw->AddRectFilled({x+col*scale,pos.y+(base+y)*scale},{x+end*scale,pos.y+(base+y+1)*scale},tint);}
        col=end;
      }
      x+=w*scale;prev=c;
    }
    return std::max(longest,x-pos.x);
  }
};
}
