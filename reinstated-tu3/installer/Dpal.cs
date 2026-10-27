using System;
using System.Collections.Generic;
using System.IO;
using System.Media;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;
using Path=System.IO.Path;

namespace PdzInstaller {
 public sealed partial class Setup {
  Canvas pageLayer; bool transitioning, sfxMuted; int shownPage=-1;
  readonly Dictionary<string,SoundPlayer> sounds=new Dictionary<string,SoundPlayer>();
  DateTime lastCursor=DateTime.MinValue, cursorQuietUntil=DateTime.MinValue;
  string tracePath; Border sweep; Button xboxLink,musicButton,sfxButton; TextBlock controlsLegend;
  static Brush Blue=new SolidColorBrush(Color.FromRgb(36,149,255));
  static Brush Ink=new SolidColorBrush(Color.FromRgb(6,24,61));
  void Trace(string text){if(tracePath!=null)try{File.AppendAllText(tracePath,DateTime.UtcNow.ToString("O")+" "+text+Environment.NewLine);}catch(IOException){}}
  void InitDpal(string[] args){
   if(Array.IndexOf(args,"--trace")>=0)tracePath=Path.Combine(Path.GetTempPath(),"pdz-setup-"+System.Diagnostics.Process.GetCurrentProcess().Id+".log");
   foreach(string cue in new[]{"select","back","cursor","error","checkbox"}){
    string path=Path.Combine(Engine.Root,"art/audio/"+cue+".wav");
    if(File.Exists(path)){var player=new SoundPlayer(path);player.Load();sounds.Add(cue,player);}
   }
   var field=new Border{Width=1344,Height=900,Background=new LinearGradientBrush(Color.FromRgb(4,13,40),Color.FromRgb(5,23,60),90)};
   Place(scene,field,128,0);
   var rings=new Canvas{Width=1100,Height=1100,Opacity=.15,IsHitTestVisible=false,RenderTransformOrigin=new Point(.5,.5)};
   for(int i=0;i<8;i++){
    double r=180+i*105;
    var ring=new Ellipse{Width=r,Height=r,Stroke=Blue,StrokeThickness=i%3==0?24:8};Place(rings,ring,550-r/2,550-r/2);
   }
   // The nested open arcs echo the native background's circle figures.
   for(int i=0;i<5;i++){
    double r=230+i*70, a=(24+i*48)*Math.PI/180, z=a+Math.PI*1.48;
    var figure=new PathFigure{StartPoint=new Point(550+r*Math.Cos(a),550+r*Math.Sin(a))};
    figure.Segments.Add(new ArcSegment(new Point(550+r*Math.Cos(z),550+r*Math.Sin(z)),new Size(r,r),0,true,SweepDirection.Clockwise,true));
    var geometry=new PathGeometry();geometry.Figures.Add(figure);
    rings.Children.Add(new System.Windows.Shapes.Path{Data=geometry,Stroke=Blue,StrokeThickness=12});
   }
   Place(scene,rings,250,-65);
   var rotation=new RotateTransform();rings.RenderTransform=rotation;
   rotation.BeginAnimation(RotateTransform.AngleProperty,new DoubleAnimation(0,360,TimeSpan.FromSeconds(140)){RepeatBehavior=RepeatBehavior.Forever});
   Place(scene,new Border{Width=1344,Height=900,Background=Lines(Color.FromArgb(80,48,113,211),6),IsHitTestVisible=false},128,0);
   foreach(int y in new[]{96,155,817,872})Place(scene,new Border{Width=1344,Height=2,Background=Blue,Opacity=.85},128,y);
   Place(scene,new Border{Width=1344,Height=57,Background=new LinearGradientBrush(Color.FromArgb(120,13,89,179),Color.FromArgb(30,16,70,139),90)},128,98);
   var wordmark=new TextBlock{Text="d-PAL",FontFamily=new FontFamily("Bahnschrift"),FontWeight=FontWeights.Bold,FontSize=47,Foreground=Blue};Place(scene,wordmark,160,32);
   Place(scene,new TextBlock{Text="PERFECT DARK ZERO",FontFamily=new FontFamily("Bahnschrift"),FontSize=21,Foreground=Blue},345,41);
   Place(scene,new TextBlock{Text="REINSTATED",FontFamily=new FontFamily("Bahnschrift"),FontSize=13,Foreground=Dim},347,67);
   xboxLink=Button("Open Perfect Dark Zero in the Xbox Store",()=>{
    string url=XboxStoreUrl(choice.UiLanguage);
    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(url){UseShellExecute=true});
    Trace("store opened "+url);
   });
   xboxLink.Width=187;xboxLink.Height=67;xboxLink.Margin=new Thickness(0);
   xboxLink.Content=new Image{Source=Load("art/dpal/xbox.png"),Stretch=Stretch.Uniform};
   var logoTemplate=new ControlTemplate(typeof(Button));
   var logoBorder=new FrameworkElementFactory(typeof(Border),"LogoOutline");
   logoBorder.SetValue(Border.BackgroundProperty,Brushes.Transparent);
   logoBorder.SetValue(Border.BorderBrushProperty,Brushes.Transparent);
   logoBorder.SetValue(Border.BorderThicknessProperty,new Thickness(1));
   logoBorder.AppendChild(new FrameworkElementFactory(typeof(ContentPresenter)));logoTemplate.VisualTree=logoBorder;
   foreach(var property in new[]{UIElement.IsMouseOverProperty,UIElement.IsKeyboardFocusedProperty}){
    var highlight=new Trigger{Property=property,Value=true};
    highlight.Setters.Add(new Setter(Border.BorderBrushProperty,Gold,"LogoOutline"));logoTemplate.Triggers.Add(highlight);
   }
   xboxLink.Template=logoTemplate;Place(scene,xboxLink,1245,19);
   foreach(string side in new[]{"left","right"}){
    var brush=new ImageBrush(Load("art/dpal/"+side+".png")){ViewboxUnits=BrushMappingMode.Absolute,Viewbox=new Rect(19,106,90,813),Stretch=Stretch.Fill};
    Place(scene,new Border{Width=108,Height=900,Background=brush},side=="left"?20:1472,0);
   }
   var light=new Ellipse{Width=15,Height=15,Fill=Lime,Stroke=new SolidColorBrush(Color.FromRgb(189,240,255)),StrokeThickness=2};Place(scene,light,1495,441);
   controlsLegend=Text("MOUSE  Select     TAB  Move     ENTER  Confirm     ESC  Back",18,Dim);Place(scene,controlsLegend,718,842);
   musicButton=Button(muted?"Music: Off":"Music: On",()=>{muted=!muted;music.IsMuted=muted;UpdateChromeLanguage();});musicButton.FontSize=15;Place(scene,musicButton,150,830);
   sfxButton=Button("SFX: On",()=>{sfxMuted=!sfxMuted;UpdateChromeLanguage();});sfxButton.FontSize=15;Place(scene,sfxButton,350,830);
   sweep=new Border{Width=1344,Height=3,Background=new LinearGradientBrush(Colors.Transparent,Color.FromRgb(100,213,255),0),Opacity=0,IsHitTestVisible=false};Place(scene,sweep,128,160);Panel.SetZIndex(sweep,100);
   Closed+=(s,e)=>{foreach(var soundPlayer in sounds.Values)soundPlayer.Dispose();Trace("closed");};
   Trace("dpal ready; loaded="+String.Join(",",sounds.Keys));
  }
  static string XboxStoreUrl(string language){
   string locale;
   switch(language){case "de":locale="de-DE";break;case "fr":locale="fr-FR";break;case "it":locale="it-IT";break;case "es":locale="es-ES";break;case "es-MX":locale="es-MX";break;case "ja":locale="ja-JP";break;case "zh":locale="zh-TW";break;case "ko":locale="ko-KR";break;default:locale="en-US";break;}
   return "https://www.xbox.com/"+locale+"/games/store/perfect-dark-zero/bx5m231l9ztn";
  }
  void UpdateXboxLink(){
   if(xboxLink==null)return;
   string label=T("Open Perfect Dark Zero in the Xbox Store");
   xboxLink.ToolTip=label+"\n"+XboxStoreUrl(choice.UiLanguage);
   System.Windows.Automation.AutomationProperties.SetName(xboxLink,label);
   System.Windows.Automation.AutomationProperties.SetHelpText(xboxLink,XboxStoreUrl(choice.UiLanguage));
  }
  void UpdateChromeLanguage(){
   UpdateXboxLink();
   if(musicButton!=null)musicButton.Content=T(muted?"Music: Off":"Music: On").ToUpperInvariant();
   if(sfxButton!=null)sfxButton.Content=T(sfxMuted?"SFX: Off":"SFX: On").ToUpperInvariant();
   if(controlsLegend!=null)controlsLegend.Text=T("MOUSE  Select     TAB  Move     ENTER  Confirm     ESC  Back");
  }
  static void Place(Canvas canvas,UIElement item,double x,double y){Canvas.SetLeft(item,x);Canvas.SetTop(item,y);canvas.Children.Add(item);}
  void PlayCue(string cue){
   if(sfxMuted)return;
   if(cue=="cursor"){
    if(transitioning||(DateTime.UtcNow-lastCursor).TotalMilliseconds<90||DateTime.UtcNow<cursorQuietUntil)return;
    lastCursor=DateTime.UtcNow;
   }else cursorQuietUntil=DateTime.UtcNow.AddMilliseconds(cue=="back"?720:cue=="error"?430:500);
   SoundPlayer sound;if(sounds.TryGetValue(cue,out sound)){try{sound.Play();Trace("sound "+cue+" page="+page);}catch(Exception e){Trace("sound failed "+e.Message);}}
  }
  void TrackCursor(Control control){control.MouseEnter+=(s,e)=>{if(control.IsEnabled)PlayCue("cursor");};control.GotKeyboardFocus+=(s,e)=>{if(control.IsEnabled)PlayCue("cursor");};}
  string ActionCue(string label){return label=="Back"||label=="Cancel"||label=="Clear"||label=="None"||label=="×"||label.StartsWith("Skip")?"back":"select";}
  Canvas BeginPage(int value){
   Canvas old=pageLayer;pageLayer=new Canvas{Width=1600,Height=900,ClipToBounds=true};scene.Children.Add(pageLayer);
   if(old!=null){old.IsHitTestVisible=false;old.Focusable=false;}
   return old;
  }
  void FinishPage(Canvas old,int value){
   bool changed=shownPage!=value;shownPage=value;
   if(!changed){if(old!=null)scene.Children.Remove(old);return;}
   // Native DarkXUI calls these TransRiseOut / TransRiseIn. Recreate the
   // rising fade in WPF; the original Xbox timeline cannot run in WPF.
   transitioning=true;pageLayer.IsHitTestVisible=false;pageLayer.Opacity=0;Canvas incoming=pageLayer;
   Trace("transition start page="+value+" rise-out=180ms rise-in=280ms delay=180ms");
   if(old!=null){var moveOut=new TranslateTransform();old.RenderTransform=moveOut;moveOut.BeginAnimation(TranslateTransform.YProperty,new DoubleAnimation(0,-32,TimeSpan.FromMilliseconds(180)));var fadeOut=new DoubleAnimation(1,0,TimeSpan.FromMilliseconds(180));fadeOut.Completed+=(s,e)=>scene.Children.Remove(old);old.BeginAnimation(OpacityProperty,fadeOut);}
   var move=new TranslateTransform(0,38);incoming.RenderTransform=move;
   move.BeginAnimation(TranslateTransform.YProperty,new DoubleAnimation(38,0,TimeSpan.FromMilliseconds(280)){BeginTime=TimeSpan.FromMilliseconds(180),EasingFunction=new CubicEase{EasingMode=EasingMode.EaseOut}});
   var fade=new DoubleAnimation(0,1,TimeSpan.FromMilliseconds(280)){BeginTime=TimeSpan.FromMilliseconds(180)};
   fade.Completed+=(s,e)=>{incoming.IsHitTestVisible=true;transitioning=false;Trace("transition complete page="+value);};
   incoming.BeginAnimation(OpacityProperty,fade);
   var beam=new DoubleAnimation(160,804,TimeSpan.FromMilliseconds(460));sweep.BeginAnimation(Canvas.TopProperty,beam);
   sweep.BeginAnimation(OpacityProperty,new DoubleAnimation(.65,0,TimeSpan.FromMilliseconds(460)));
  }
 }
}
