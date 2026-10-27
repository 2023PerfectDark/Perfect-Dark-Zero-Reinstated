#include "bug_report.h"
#include "utils.h"
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/window.h>
#include <rex/ui/presenter.h>
#include <imgui.h>
#include <Windows.h>
#include <winhttp.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <atomic>
#include <future>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace pdz_report {
namespace {
std::atomic<bool> open=false;
using Microsoft::WRL::ComPtr;
void Check(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("Screenshot encoding failed. Try capturing again."); }
std::vector<unsigned char> Png(const rex::ui::RawImage& image) {
  if(!image.width || !image.height || image.data.empty()) throw std::runtime_error("No game frame is available yet.");
  const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  struct ComEnd { bool active; ~ComEnd(){if(active)CoUninitialize();} } end{SUCCEEDED(hr)};
  ComPtr<IWICImagingFactory> factory;
  Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
  ComPtr<IStream> stream; Check(CreateStreamOnHGlobal(nullptr,TRUE,&stream));
  ComPtr<IWICBitmapEncoder> encoder; Check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
  Check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
  ComPtr<IWICBitmapFrameEncode> frame; Check(encoder->CreateNewFrame(&frame,nullptr));
  Check(frame->Initialize(nullptr)); Check(frame->SetSize(image.width,image.height));
  WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR; Check(frame->SetPixelFormat(&format));
  std::vector<unsigned char> pixels(size_t(image.width)*image.height*3);
  for(size_t y=0;y<image.height;++y)for(size_t x=0;x<image.width;++x){
    const auto* src=image.data.data()+y*image.stride+x*4; auto* dst=pixels.data()+(y*image.width+x)*3;
    dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];
  }
  Check(frame->WritePixels(image.height,image.width*3,UINT(pixels.size()),pixels.data()));
  Check(frame->Commit()); Check(encoder->Commit());
  STATSTG stat{};Check(stream->Stat(&stat,STATFLAG_NONAME));
  if(stat.cbSize.QuadPart>25*1024*1024)throw std::runtime_error("Screenshot exceeds 25 MiB. Lower the rendering resolution and capture again.");
  LARGE_INTEGER zero{};Check(stream->Seek(zero,STREAM_SEEK_SET,nullptr));
  std::vector<unsigned char> bytes(size_t(stat.cbSize.QuadPart)); ULONG count=0;
  Check(stream->Read(bytes.data(),ULONG(bytes.size()),&count));
  if(count!=bytes.size())throw std::runtime_error("Could not read the screenshot.");
  return bytes;
}
struct Internet { HINTERNET h; ~Internet(){if(h)WinHttpCloseHandle(h);} operator HINTERNET() const{return h;} };
std::string Request(HINTERNET connection,const wchar_t* method,const wchar_t* path,const std::wstring& headers,const std::string& body,DWORD& status) {
  Internet request{WinHttpOpenRequest(connection,method,path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
  if(!request.h)throw std::runtime_error("Could not connect to the inbox.");
  DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
  if(!WinHttpSendRequest(request,headers.c_str(),DWORD(headers.size()),body.empty()?nullptr:const_cast<char*>(body.data()),DWORD(body.size()),DWORD(body.size()),0)||!WinHttpReceiveResponse(request,nullptr))
    throw std::runtime_error("Connection failed. Your draft is kept. If sending had started, check the inbox before retrying to avoid duplicates.");
  DWORD size=sizeof(status);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr);
  std::string response;char buffer[8192];DWORD read=0;
  do{if(!WinHttpReadData(request,buffer,sizeof(buffer),&read))throw std::runtime_error("Could not read the inbox response. Check the inbox before retrying.");response.append(buffer,read);
    if(response.size()>1024*1024)throw std::runtime_error("Unexpected inbox response.");}while(read);
  return response;
}
std::string Send(std::string title,std::string notes,std::string steps,std::string category,std::vector<unsigned char> png) {
  Internet session{WinHttpOpen(L"PDZ-Reinstated-Report/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
  if(!session.h)throw std::runtime_error("Could not start the report connection.");
  WinHttpSetTimeouts(session,10000,10000,30000,30000);
  Internet connection{WinHttpConnect(session,L"perfectdarkmods.duckdns.org",INTERNET_DEFAULT_HTTPS_PORT,0)};
  if(!connection.h)throw std::runtime_error("Could not connect to the inbox.");
  DWORD status=0;auto html=Request(connection,L"GET",L"/",L"","",status);
  const std::string marker="name=\"csrf-token\" content=\"";auto start=html.find(marker);
  if(status!=200||start==std::string::npos)throw std::runtime_error("The inbox form is unavailable. Your draft is kept.");
  start+=marker.size();auto finish=html.find('"',start);auto token=html.substr(start,finish-start);
  if(token.empty()||token.size()>128||token.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::string::npos)
    throw std::runtime_error("Unexpected inbox form. Your draft is kept.");
  const std::string boundary="PDZReport"+std::to_string(GetTickCount64());std::string body;
  auto field=[&](const char* name,const std::string& value){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\""+name+"\"\r\n\r\n"+value+"\r\n";};
  field("kind","Bug report");field("game","Perfect Dark Zero Reinstated");field("mod","PDZ Reinstated");
  field("version","F6 report build " __DATE__ " " __TIME__);field("title",title);field("actual",notes);field("steps",steps);field("category",category);field("consent","on");
  if(!png.empty()){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\"attachments\"; filename=\"game.png\"\r\nContent-Type: image/png\r\n\r\n";body.append(reinterpret_cast<const char*>(png.data()),png.size());body+="\r\n";}
  body+="--"+boundary+"--\r\n";
  std::wstring headers=L"Origin: https://perfectdarkmods.duckdns.org\r\nX-CSRF-Token: "+std::wstring(token.begin(),token.end())+L"\r\nContent-Type: multipart/form-data; boundary="+std::wstring(boundary.begin(),boundary.end())+L"\r\n";
  auto response=Request(connection,L"POST",L"/api/reports",headers,body,status);
  if(status!=201)throw std::runtime_error(status==429?"Too many reports. Try again in an hour; your draft is kept.":"The inbox rejected the report (HTTP "+std::to_string(status)+"). Your draft is kept.");
  auto id=response.find("BUG-");if(id==std::string::npos||response.size()<id+20)throw std::runtime_error("The server replied, but no reference was found. Check the inbox before retrying.");
  return response.substr(id,20);
}
class Dialog final:public rex::ui::ImGuiDialog {
 public:
  Dialog(rex::ui::ImGuiDrawer* drawer,std::function<rex::ui::Presenter*()> presenter):ImGuiDialog(drawer),presenter_(presenter){}
 private:
  std::function<rex::ui::Presenter*()> presenter_;char title_[161]{},notes_[3001]{},steps_[6001]{};int category_=0;
  bool consent_=false;std::vector<unsigned char> screenshot_;std::string message_;std::future<std::string> sending_;
  void OnDraw(ImGuiIO&) override {
    if(sending_.valid()&&sending_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
      try{message_="Report received: "+sending_.get();title_[0]=notes_[0]=steps_[0]=0;screenshot_.clear();consent_=false;}catch(const std::exception& e){message_=e.what();}
    }
    if(!open)return;
    bool visible=true;ImGui::SetNextWindowSize({720,620},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Report a bug (F6)",&visible)){
      ImGui::TextWrapped("The old F6 mouse and FOV settings moved to Extended Options in the in-game menu.");
      ImGui::TextWrapped("Send a screenshot and notes to the maintainer's private inbox at perfectdarkmods.duckdns.org.");
      const bool busy=sending_.valid();ImGui::BeginDisabled(busy);
      if(ImGui::Button(screenshot_.empty()?"Take screenshot":"Retake screenshot")){
        try{rex::ui::RawImage image;auto* presenter=presenter_();if(!presenter||!presenter->CaptureGuestOutput(image))throw std::runtime_error("No game frame is available. Return to gameplay and try again.");screenshot_=Png(image);message_="Game screenshot attached. The report dialog and desktop are excluded.";}catch(const std::exception& e){message_=e.what();}
      }
      if(!screenshot_.empty()){ImGui::SameLine();if(ImGui::Button("Remove screenshot"))screenshot_.clear();}
      ImGui::InputText("Title",title_,sizeof(title_));
      ImGui::Combo("Category",&category_,"Gameplay\0Graphics\0Audio\0Crash\0Menus / controls\0Installation\0Other\0");
      ImGui::InputTextMultiline("What happened?",notes_,sizeof(notes_),{0,110});
      ImGui::InputTextMultiline("How to reproduce",steps_,sizeof(steps_),{0,90});
      ImGui::TextWrapped("Only these notes, the attached game screenshot, category, and build version are sent. Remove private details before sending.");
      ImGui::Checkbox("I agree to share this report with the maintainer and their debugging tools, including AI.",&consent_);
      ImGui::BeginDisabled(!consent_||!title_[0]||!notes_[0]||!steps_[0]);
      if(ImGui::Button("Send bug report")){
        static const char* categories[]={"Gameplay","Graphics","Audio","Crash","Menus / controls","Installation","Other"};
        sending_=std::async(std::launch::async,Send,std::string(title_),std::string(notes_),std::string(steps_),std::string(categories[category_]),screenshot_);message_="Sending report...";
      }
      ImGui::EndDisabled();ImGui::EndDisabled();ImGui::SameLine();
      if(ImGui::Button(busy?"Return to game":"Cancel / return to game"))open=false;
      if(!message_.empty())ImGui::TextWrapped("%s",message_.c_str());
    }ImGui::End();if(!visible)open=false;
  }
};
}
void Open(){open=!open.load();}
bool IsOpen(){return open.load();}
std::unique_ptr<rex::ui::ImGuiDialog> MakeDialog(rex::ui::ImGuiDrawer* drawer,std::function<rex::ui::Presenter*()> presenter){return std::make_unique<Dialog>(drawer,presenter);}
}


