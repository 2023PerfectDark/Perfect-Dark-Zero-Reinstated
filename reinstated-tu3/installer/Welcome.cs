using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Threading;

namespace PdzInstaller {
 public sealed partial class Setup {
  ScrollViewer bodyScroll;
  DispatcherTimer welcomeTimer;
  Button scrollPause;
  bool welcomePaused;
  DateTime welcomeLastTick, welcomeStart;

  void Welcome() {
   title.Text=T("This is Perfect Dark Zero: Reinstated!");
   body.Children.Add(Text("We aim to make the game more modern for its time and make it more playable to fans who disliked the more Hardcore semi Tactical shooter nature of the game, and not play like its past counterparts like GE & PD. This Enhancement fixes that while preserving the original games concept. While adding.......New Stuff. Want a hint? Ok.",24));
   body.Children.Add(Text("Are you a Modern Agent, an Old School Agent, or an OG Dark Zero Agent?",24));
   body.Children.Add(Text("Use your US original or US Rev 1 Xbox 360 copy of Perfect Dark Zero. Rev 1 includes both map packs and needs no separate title update.",28,Gold));
   var musicFix=Text("Perfect Dark Zero: Reinstated Title Update fixes broken music that is present in the Limited Edition, Rare Replay, and Official Xbox 360/XBone Title Update versions",22);
   string message=musicFix.Text,emphasis=T("Title Update");
   int emphasisAt=message.IndexOf(emphasis,StringComparison.OrdinalIgnoreCase);
   if(emphasisAt>=0){
    musicFix.Inlines.Clear();
    musicFix.Inlines.Add(new Run(message.Substring(0,emphasisAt)));
    musicFix.Inlines.Add(new Run(message.Substring(emphasisAt,emphasis.Length)){Foreground=Gold,FontWeight=FontWeights.Bold});
    musicFix.Inlines.Add(new Run(message.Substring(emphasisAt+emphasis.Length)));
   }
   body.Children.Add(musicFix);
   Note("The game disc is verified before installation. You can supply an ISO directly or choose an extracted game folder.");
   Note("Title Update 3, DLC, extra languages and the Bonus DVD are optional.");
   welcomePaused=false;
   scrollPause=Button("Pause Scroll",()=>{
    if(welcomePaused){
     if(bodyScroll.VerticalOffset>=bodyScroll.ScrollableHeight-1)bodyScroll.ScrollToTop();
     welcomePaused=false;welcomeStart=DateTime.UtcNow.AddSeconds(2);welcomeLastTick=DateTime.UtcNow;
     scrollPause.Content=T("Pause Scroll").ToUpperInvariant();welcomeTimer.Start();Trace("welcome scroll resumed");
    }else PauseWelcome();
   });
   actions.Children.Insert(0,scrollPause);
   bodyScroll.PreviewMouseWheel+=(s,e)=>PauseWelcome();
   bodyScroll.PreviewMouseDown+=(s,e)=>PauseWelcome();
   bodyScroll.PreviewKeyDown+=(s,e)=>{
    if(e.Key==Key.Up||e.Key==Key.Down||e.Key==Key.PageUp||e.Key==Key.PageDown||e.Key==Key.Home||e.Key==Key.End)PauseWelcome();
   };
   welcomeStart=DateTime.UtcNow.AddSeconds(4);welcomeLastTick=DateTime.UtcNow;
   welcomeTimer=new DispatcherTimer{Interval=TimeSpan.FromMilliseconds(33)};
   welcomeTimer.Tick+=(s,e)=>{
    var now=DateTime.UtcNow;double seconds=Math.Min(.1,(now-welcomeLastTick).TotalSeconds);welcomeLastTick=now;
    if(welcomePaused||now<welcomeStart||transitioning||bodyScroll.ScrollableHeight<=0)return;
    bodyScroll.ScrollToVerticalOffset(Math.Min(bodyScroll.ScrollableHeight,bodyScroll.VerticalOffset+seconds*12));
    if(bodyScroll.VerticalOffset>=bodyScroll.ScrollableHeight-1){PauseWelcome();scrollPause.Content=T("Replay Scroll").ToUpperInvariant();Trace("welcome scroll reached bottom");}
   };
   welcomeTimer.Start();Trace("welcome auto scroll scheduled");
  }
  void PauseWelcome(){
   if(welcomePaused||page!=1)return;
   welcomePaused=true;if(welcomeTimer!=null)welcomeTimer.Stop();
   scrollPause.Content=T("Resume Scroll").ToUpperInvariant();Trace("welcome scroll paused at "+bodyScroll.VerticalOffset.ToString("0.0"));
  }
  void StopWelcome(){if(welcomeTimer!=null){welcomeTimer.Stop();welcomeTimer=null;}}
 }
}
