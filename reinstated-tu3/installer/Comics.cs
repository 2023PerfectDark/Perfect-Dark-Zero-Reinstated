using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace PdzInstaller {
 public sealed partial class Setup {
  Canvas comicGallery;
  Button comicPause;
  bool comicsPaused;
  readonly List<KeyValuePair<FrameworkElement,Storyboard>> comicFades=new List<KeyValuePair<FrameworkElement,Storyboard>>();

  void ComicsPage(){
   panel.Visibility=Visibility.Collapsed;stepLabel.Visibility=Visibility.Collapsed;
   title=Text("Did you know there is a Perfect Dark Zero Comic?",34);title.Width=1260;At(title,165,202);
   var hint=Text("Click a picture to pause and read full-screen. Space pauses / resumes.",18,Dim);At(hint,165,245);
   actions.Children.Clear();comicIndex=0;comicsPaused=false;comicFades.Clear();
   comicPause=Button("Pause",()=>SetComicsPaused(!comicsPaused));actions.Children.Add(comicPause);
   actions.Children.Add(Button("Replay",()=>Show(11)));actions.Children.Add(Button("Finish",()=>Close()));
   comicGallery=new Canvas{Width=1270,Height=462,ClipToBounds=true};At(comicGallery,165,277);
   comicTimer=new DispatcherTimer{Interval=TimeSpan.FromSeconds(4)};comicTimer.Tick+=(s,e)=>AdvanceComic();
   AdvanceComic();comicTimer.Start();
  }
  void AdvanceComic(){
   if(comicIndex>=art.comics.Length){comicTimer.Stop();return;}
   int index=comicIndex++;bool final=index==art.comics.Length-1;
   var bitmap=Load(art.comics[index]);
   var frame=Button("Read comic page",()=>ReadComic(bitmap,index));
   frame.Content=new Image{Source=bitmap,Stretch=Stretch.Uniform};
   frame.Background=Brushes.Black;frame.BorderBrush=Dim;frame.Margin=new Thickness(0);
   frame.Width=final?960:440;frame.Height=final?454:435;
   AutomationProperties.SetName(frame,T("Read comic page")+" "+(index+1));
   frame.ToolTip=T("Click to pause and read full-screen");
   var template=new ControlTemplate(typeof(Button));
   var border=new FrameworkElementFactory(typeof(Border));border.SetValue(Border.BackgroundProperty,Brushes.Black);
   border.SetValue(Border.BorderBrushProperty,Dim);border.SetValue(Border.BorderThicknessProperty,new Thickness(1));
   var content=new FrameworkElementFactory(typeof(ContentPresenter));content.SetValue(FrameworkElement.MarginProperty,new Thickness(4));
   border.AppendChild(content);template.VisualTree=border;frame.Template=template;
   Canvas.SetLeft(frame,final?155:(index%3)*415);Canvas.SetTop(frame,final?4:(index%2)*14);
   Panel.SetZIndex(frame,index+1);comicGallery.Children.Add(frame);
   var fade=new Storyboard();var animation=new DoubleAnimation(0,1,TimeSpan.FromSeconds(1));
   Storyboard.SetTargetProperty(animation,new PropertyPath(OpacityProperty));fade.Children.Add(animation);
   fade.Begin(frame,true);comicFades.Add(new KeyValuePair<FrameworkElement,Storyboard>(frame,fade));
   Trace("comic "+(index+1)+" retained="+comicGallery.Children.Count+" final="+final);
   if(final)comicTimer.Stop();
  }
  void SetComicsPaused(bool pause){
   comicsPaused=pause;
   if(pause)comicTimer.Stop();else if(comicIndex<art.comics.Length)comicTimer.Start();
   foreach(var fade in comicFades){if(pause)fade.Value.Pause(fade.Key);else fade.Value.Resume(fade.Key);}
   comicPause.Content=T(pause?"Resume":"Pause").ToUpperInvariant();Trace("comics "+(pause?"paused":"resumed")+" at "+comicIndex);
  }
  void ReadComic(BitmapSource bitmap,int index){
   SetComicsPaused(true);
   var reader=new Window{Title="Perfect Dark Zero — "+T("Read comic page")+" "+(index+1),Owner=this,
    WindowStyle=WindowStyle.None,ResizeMode=ResizeMode.NoResize,Background=Brushes.Black,
    WindowStartupLocation=WindowStartupLocation.CenterOwner,Width=ActualWidth,Height=ActualHeight};
   reader.Resources.Add(typeof(Button),ButtonStyle());
   var root=new DockPanel();reader.Content=root;
   var toolbar=new WrapPanel{Background=Ink,Margin=new Thickness(0),HorizontalAlignment=HorizontalAlignment.Center};
   DockPanel.SetDock(toolbar,Dock.Top);root.Children.Add(toolbar);
   var image=new Image{Source=bitmap,Stretch=Stretch.Fill,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Top,Margin=new Thickness(16)};
   var scroll=new ScrollViewer{Content=image,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Auto};root.Children.Add(scroll);
   double scale=1;bool fit=true;
   var zoomLabel=Text("100%",18,Dim);zoomLabel.VerticalAlignment=VerticalAlignment.Center;zoomLabel.Margin=new Thickness(12,0,12,0);
   Action<double> zoom=(value)=>{scale=Math.Max(.1,Math.Min(4,value));image.Width=bitmap.PixelWidth*scale;image.Height=bitmap.PixelHeight*scale;zoomLabel.Text=Math.Round(scale*100)+"%";};
   Action fitPage=()=>{fit=true;zoom(Math.Min(Math.Max(1,scroll.ActualWidth-50)/bitmap.PixelWidth,Math.Max(1,scroll.ActualHeight-34)/bitmap.PixelHeight));scroll.ScrollToTop();scroll.ScrollToLeftEnd();};
   toolbar.Children.Add(Button("Back to gallery",()=>reader.Close()));
   toolbar.Children.Add(Button("Fit page",()=>fitPage()));
   toolbar.Children.Add(Button("100%",()=>{fit=false;zoom(1);}));
   toolbar.Children.Add(Button("−",()=>{fit=false;zoom(scale/1.25);}));toolbar.Children.Add(zoomLabel);
   toolbar.Children.Add(Button("+",()=>{fit=false;zoom(scale*1.25);}));
   var help=Text("Scroll to read • Esc to return",18,Dim);help.VerticalAlignment=VerticalAlignment.Center;help.Margin=new Thickness(15,0,15,0);toolbar.Children.Add(help);
   scroll.SizeChanged+=(s,e)=>{if(fit)fitPage();};
   scroll.PreviewMouseWheel+=(s,e)=>{if((Keyboard.Modifiers&ModifierKeys.Control)!=0){fit=false;zoom(scale*(e.Delta>0?1.25:.8));e.Handled=true;}};
   reader.PreviewKeyDown+=(s,e)=>{if(e.Key==Key.Escape){PlayCue("back");reader.Close();e.Handled=true;}else if(e.Key==Key.Space){e.Handled=true;}};
   reader.Loaded+=(s,e)=>{reader.WindowState=WindowState.Maximized;fitPage();};
   Trace("comic reader opened "+(index+1));reader.ShowDialog();Trace("comic reader closed; slideshow remains paused");
  }
 }
}
