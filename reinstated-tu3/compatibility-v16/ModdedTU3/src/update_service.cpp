#include "update_service.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <SDL3/SDL.h>

namespace pdz_update {
namespace {
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
std::recursive_mutex mutex;
Status status;
HANDLE worker=nullptr;
bool waiting=true, restarting=false;
Clock::time_point deadline{}, lastPoll{};
fs::path Root() { wchar_t path[32768]{}; GetModuleFileNameW(nullptr,path,32768); return fs::path(path).parent_path(); }
bool Launch(const wchar_t* action) {
  try {
    const auto root=Root(), work=root/L".pdz-update";
    fs::create_directories(work);
    auto script=root/L"mod_assets/updater/update.ps1";
    if(!fs::is_regular_file(script))throw std::runtime_error("Update helper is missing.");
    // Keep the installer independent of files it will replace.
    if(std::wstring_view(action)==L"Install") {
      fs::copy_file(script,work/L"install.ps1",fs::copy_options::overwrite_existing);
      script=work/L"install.ps1";
    }
    wchar_t system[32768]{}; GetSystemDirectoryW(system,32768);
    const auto shell=fs::path(system)/L"WindowsPowerShell/v1.0/powershell.exe";
    std::wstring command=L"\""+shell.wstring()+L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+
      script.wstring()+L"\" -Action "+action+L" -Root \""+root.wstring()+L"\" -GameProcessId "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(shell.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&si,&pi))
      throw std::runtime_error("Could not start the update helper.");
    CloseHandle(pi.hThread);worker=pi.hProcess;
    return true;
  } catch(const std::exception& e) { status={"error",e.what()};return false; }
}
bool Active() { return worker&&WaitForSingleObject(worker,0)==WAIT_TIMEOUT; }
void Start(const wchar_t* action) {
  if(Active()||restarting)return;
  if(worker){CloseHandle(worker);worker=nullptr;}
  waiting=false;deadline={};
  std::error_code ec;fs::remove(Root()/L".pdz-update/status.txt",ec);
  status={"checking","Checking for updates..."};Launch(action);
}
}
void Check(){std::lock_guard lock(mutex);if(status.phase!="ready")Start(L"Check");}
bool OpenReportBoard(){return SDL_OpenURL("https://texturepacks.art/pdghosts/board");}
void Download(){std::lock_guard lock(mutex);if(status.phase=="available")Start(L"Download");}
void Redownload(){std::lock_guard lock(mutex);Start(L"Redownload");}
void Wait(){std::lock_guard lock(mutex);waiting=true;status.seconds=-1;}
void Restart(){
  std::lock_guard lock(mutex);
  if(status.phase!="ready"||Active()||restarting)return;
  if(worker){CloseHandle(worker);worker=nullptr;}
  // Find our actual game window before starting a helper that waits for exit.
  HWND window=nullptr;
  EnumWindows([](HWND h,LPARAM p)->BOOL { DWORD pid=0;GetWindowThreadProcessId(h,&pid);
    if(pid==GetCurrentProcessId()&&IsWindowVisible(h)&&GetWindow(h,GW_OWNER)==nullptr){*reinterpret_cast<HWND*>(p)=h;return FALSE;}return TRUE;
  },reinterpret_cast<LPARAM>(&window));
  if(!window){status={"error","Could not locate the game window. Restart has been cancelled."};return;}
  if(Launch(L"Install")){restarting=true;status={"installing","Closing the game to install the update..."};PostMessageW(window,WM_CLOSE,0,0);}
}
Status Poll(){
  std::lock_guard lock(mutex);const auto now=Clock::now();
  if(now-lastPoll>std::chrono::milliseconds(200)&&!restarting){
    lastPoll=now;
    std::ifstream file(Root()/L".pdz-update/status.txt");
    std::string phase,percent,message;
    if(std::getline(file,phase)&&std::getline(file,percent)&&std::getline(file,message)){
      if(phase.starts_with("\xef\xbb\xbf"))phase.erase(0,3);
      if(!phase.empty()&&phase.back()=='\r')phase.pop_back();
      try {
        const int value=std::clamp(std::stoi(percent),0,100);
        if(phase=="ready"&&status.phase!="ready"){deadline=now+std::chrono::seconds(10);}
        if(phase!="installed")status={phase,message.substr(0,220),value};
      } catch(...){}
    }
    if(worker&&!Active()){
      DWORD code=0;GetExitCodeProcess(worker,&code);CloseHandle(worker);worker=nullptr;
      if(status.phase=="checking"||status.phase=="downloading"||status.phase=="verifying")status={"error","The update helper stopped before completing. Please retry."};
    }
  }
  if(status.phase=="ready"&&!waiting){
    status.seconds=std::max(0,int(std::chrono::ceil<std::chrono::seconds>(deadline-now).count()));
    if(status.seconds==0)Restart();
  }
  return status;
}
bool Successful(){std::error_code ec;return fs::is_regular_file(Root()/L".pdz-update/success.json",ec);}
void Acknowledge(){std::error_code ec;fs::remove(Root()/L".pdz-update/success.json",ec);}
std::vector<std::u16string> ReadChangelog(){
  try {
    std::ifstream file(Root()/L"mod_assets/updater/changelog.txt",std::ios::binary);
    if(!file)return {u"No changelog found. Edit mod_assets/updater/changelog.txt to add release notes."};
    // Bounded, read only on opening/reloading the page, never per render frame.
    std::string bytes(65536,'\0');file.read(bytes.data(),bytes.size());bytes.resize(size_t(file.gcount()));
    if(bytes.starts_with("\xef\xbb\xbf"))bytes.erase(0,3);
    int count=MultiByteToWideChar(CP_UTF8,0,bytes.data(),int(bytes.size()),nullptr,0);
    std::wstring decoded(size_t(count),L'\0');
    if(count)MultiByteToWideChar(CP_UTF8,0,bytes.data(),int(bytes.size()),decoded.data(),count);
    std::u16string text;
    for(wchar_t c:decoded){
      if(c<=L' '){if(!text.empty()&&text.back()!=u' ')text+=u' ';}
      else text+=char16_t(c);
    }
    std::vector<std::u16string> pages;
    for(size_t start=0;start<text.size();){
      while(start<text.size()&&text[start]==u' ')++start;
      if(start==text.size())break;
      size_t end=std::min(start+180,text.size());
      if(end<text.size()){
        const auto space=text.rfind(u' ',end);
        if(space!=std::u16string::npos&&space>start)end=space;
        // Do not split a UTF-16 surrogate pair when a long word is wrapped.
        if(end>start&&text[end-1]>=0xD800&&text[end-1]<=0xDBFF)--end;
      }
      pages.emplace_back(text.substr(start,end-start));start=end;
    }
    if(pages.empty())pages.emplace_back(u"No release notes yet. Edit mod_assets/updater/changelog.txt, then select Reload.");
    return pages;
  }catch(...){return {u"Could not read mod_assets/updater/changelog.txt. Save it as a UTF-8 text file and select Reload."};}
}
}
