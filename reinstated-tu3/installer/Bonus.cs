using System;
using System.IO;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
namespace PdzBonus {
 sealed class Player : Form {
  IntPtr instance,media,player; readonly Panel video=new Panel{Dock=DockStyle.Fill,BackColor=Color.Black}; readonly Timer timer=new Timer{Interval=40}; ushort previous;int ticks;string source;bool probe,playing;string[] movies;
  readonly ListBox menu=new ListBox{Dock=DockStyle.Fill,BackColor=Color.FromArgb(3,20,8),ForeColor=Color.YellowGreen,Font=new Font("Segoe UI",20),BorderStyle=BorderStyle.None,IntegralHeight=false};
  readonly TrackBar seek=new TrackBar{Dock=DockStyle.Fill,Minimum=0,Maximum=1000,TickStyle=TickStyle.None,Enabled=false,AccessibleName="Seek position",BackColor=Color.FromArgb(7,20,8)};
  readonly Label elapsed=new Label{Dock=DockStyle.Right,Width=160,Text="00:00 / 00:00",TextAlign=ContentAlignment.MiddleCenter,ForeColor=Color.LightGreen,AccessibleName="Playback time"};
  Button rewind,forward;bool seeking;long duration,requestedTime;DateTime seekDisplayUntil;
  [DllImport("kernel32",CharSet=CharSet.Unicode)] static extern bool SetDllDirectory(string path);
  [DllImport("kernel32",CharSet=CharSet.Unicode)] static extern IntPtr LoadLibrary(string path);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr libvlc_new(int count,IntPtr args);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr libvlc_media_new_location(IntPtr lib,byte[] path);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr libvlc_media_new_path(IntPtr lib,byte[] path);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern IntPtr libvlc_media_player_new_from_media(IntPtr media);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_add_option(IntPtr media,byte[] option);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_player_set_hwnd(IntPtr player,IntPtr window);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern int libvlc_media_player_play(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern int libvlc_media_player_get_state(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern int libvlc_video_get_size(IntPtr player,uint num,out uint width,out uint height);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_player_navigate(IntPtr player,uint mode);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_player_pause(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern long libvlc_media_player_get_time(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern long libvlc_media_player_get_length(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_player_set_time(IntPtr player,long milliseconds);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern int libvlc_media_player_is_seekable(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_player_stop(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_player_release(IntPtr player);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_media_release(IntPtr media);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_release(IntPtr instance);
  [DllImport("libvlc",CallingConvention=CallingConvention.Cdecl)] static extern void libvlc_video_set_key_input(IntPtr player,uint value);
  [DllImport("xinput1_4.dll")] static extern uint XInputGetState(uint user,out Pad state);
  [StructLayout(LayoutKind.Sequential)] struct Pad {public uint packet;public ushort buttons;public byte leftTrigger,rightTrigger;public short lx,ly,rx,ry;}
  static byte[] Utf8(string s){return Encoding.UTF8.GetBytes(s+"\0");}
  [STAThread] static void Main(string[] args){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);try{Application.Run(new Player(args));}catch(Exception e){File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"bonus-error.txt"),e.ToString());MessageBox.Show(e.Message,"PDZ Bonus DVD");}}
  Player(string[] args){Text="Perfect Dark Zero — Bonus DVD";Width=1100;Height=760;MinimumSize=new Size(700,500);StartPosition=FormStartPosition.CenterScreen;KeyPreview=true;
   source=args.Length>0&&!args[0].StartsWith("--")?Path.GetFullPath(args[0]):Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"bonus","contents");probe=Array.IndexOf(args,"--probe")>=0;
   var footer=new FlowLayoutPanel{Dock=DockStyle.Bottom,Height=90,BackColor=Color.FromArgb(7,20,8),Padding=new Padding(8)};
   var help=new Label{Text="Up/Down: Move   A / Enter: Play   B / M: Menu   Start / Space: Pause\nLeft/Right or D-pad: Seek 10 seconds   Drag the seek bar to jump",ForeColor=Color.LightGreen,AutoSize=true,Margin=new Padding(5,3,15,5)};footer.Controls.Add(help);footer.SetFlowBreak(help,true);
   foreach(var text in new[]{"Menu","Pause / Play","-10 seconds","+10 seconds","Exit"}){string label=text;var b=new Button{Text=label,AutoSize=true,BackColor=Color.FromArgb(25,65,20),ForeColor=Color.White,FlatStyle=FlatStyle.Flat};b.Click+=(s,e)=>{if(label=="Exit")Close();else if(label=="Menu")Back();else if(label=="-10 seconds")SeekBy(-10000);else if(label=="+10 seconds")SeekBy(10000);else if(playing)libvlc_media_player_pause(player);else Play();};if(label=="-10 seconds")rewind=b;if(label=="+10 seconds")forward=b;footer.Controls.Add(b);}
   var timeline=new Panel{Dock=DockStyle.Bottom,Height=44,BackColor=Color.FromArgb(7,20,8),Padding=new Padding(12,5,12,0)};timeline.Controls.Add(seek);timeline.Controls.Add(elapsed);
   seek.MouseDown+=(s,e)=>{if(e.Button==MouseButtons.Left&&seek.Enabled){seeking=true;seek.Capture=true;SeekPreview(e.X);}};
   seek.MouseMove+=(s,e)=>{if(seeking)SeekPreview(e.X);};
   seek.MouseUp+=(s,e)=>{if(seeking&&e.Button==MouseButtons.Left){SeekPreview(e.X);seeking=false;SeekTo(duration*seek.Value/1000);seek.Capture=false;}};
   seek.MouseCaptureChanged+=(s,e)=>{if(!seek.Capture&&seeking){seeking=false;SeekTo(duration*seek.Value/1000);}};
   seek.Scroll+=(s,e)=>{if(!seeking)SeekTo(duration*seek.Value/1000);};
   Controls.Add(video);Controls.Add(menu);Controls.Add(timeline);Controls.Add(footer);ResetSeek();
   var heading=new Label{Text="PERFECT DARK ZERO  /  BONUS DVD\nChoose a feature",Font=new Font("Segoe UI",24,FontStyle.Bold),ForeColor=Color.Gold,BackColor=Color.FromArgb(3,20,8),Dock=DockStyle.Top,Height=120,Padding=new Padding(20)};Controls.Add(heading);
   menu.DoubleClick+=(s,e)=>Play();Shown+=(s,e)=>Start();timer.Tick+=(s,e)=>Poll();FormClosed+=(s,e)=>{timer.Stop();Release();if(instance!=IntPtr.Zero)libvlc_release(instance);};
  }
  void Start(){try{if(!Directory.Exists(source))throw new IOException("The Bonus DVD content was not found. Install the Bonus DVD option first.");var list=new System.Collections.Generic.List<string>();foreach(var path in Directory.GetFiles(source,"*",SearchOption.AllDirectories)){string ext=Path.GetExtension(path).ToLowerInvariant();if((ext==".wmv"||ext==".bik"||ext==".wma")&&!Path.GetFileName(path).StartsWith("MSGS_"))list.Add(path);}list.Sort();movies=list.ToArray();foreach(var path in movies){string n=Path.GetFileNameWithoutExtension(path).Replace("_MASTER","").Replace("_"," ");if(n.Equals("puntercomplete",StringComparison.OrdinalIgnoreCase))n="Kameo Elements of Power Trailer";if(n=="PDZBD Background")n="Bonus disc background music";menu.Items.Add(n);}if(movies.Length==0)throw new IOException("No bonus videos found.");menu.SelectedIndex=0;string lib=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"bonus","vlc");if(!File.Exists(Path.Combine(lib,"libvlc.dll")))throw new IOException("The playback components are missing. Reinstall the Bonus DVD option.");
    SetDllDirectory(lib);Environment.SetEnvironmentVariable("VLC_PLUGIN_PATH",Path.Combine(lib,"plugins"));if(LoadLibrary(Path.Combine(lib,"libvlccore.dll"))==IntPtr.Zero||LoadLibrary(Path.Combine(lib,"libvlc.dll"))==IntPtr.Zero)throw new IOException("Could not load the 64-bit DVD player.");instance=libvlc_new(0,IntPtr.Zero);if(instance==IntPtr.Zero)throw new IOException("Could not initialize DVD playback.");
    timer.Start();menu.Focus();if(probe)Play();
   }catch(Exception e){MessageBox.Show(this,e.Message,"Bonus DVD",MessageBoxButtons.OK,MessageBoxIcon.Error);Close();}}
  void Release(){seeking=false;seek.Capture=false;if(player!=IntPtr.Zero){libvlc_media_player_stop(player);libvlc_media_player_release(player);player=IntPtr.Zero;}if(media!=IntPtr.Zero){libvlc_media_release(media);media=IntPtr.Zero;}ResetSeek();}
  void ResetSeek(){duration=0;seek.Value=0;seek.Enabled=false;rewind.Enabled=false;forward.Enabled=false;elapsed.Text="00:00 / 00:00";seekDisplayUntil=DateTime.MinValue;}
  static string Clock(long ms){var t=TimeSpan.FromMilliseconds(Math.Max(0,ms));return t.TotalHours>=1?t.ToString(@"h\:mm\:ss"):t.ToString(@"mm\:ss");}
  void SeekPreview(int x){seek.Value=(int)Math.Max(0,Math.Min(1000,(x-13)*1000.0/Math.Max(1,seek.ClientSize.Width-26)));elapsed.Text=Clock(duration*seek.Value/1000)+" / "+Clock(duration);}
  void SeekTo(long time){if(!playing||player==IntPtr.Zero||duration<=0||libvlc_media_player_is_seekable(player)==0)return;requestedTime=Math.Max(0,Math.Min(Math.Max(0,duration-250),time));seekDisplayUntil=DateTime.UtcNow.AddMilliseconds(800);libvlc_media_player_set_time(player,requestedTime);if(probe)File.AppendAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"bonus-probe.txt"),"seek="+requestedTime+" state="+libvlc_media_player_get_state(player)+Environment.NewLine);UpdateSeek();}
  void SeekBy(long delta){if(player!=IntPtr.Zero)SeekTo((DateTime.UtcNow<seekDisplayUntil?requestedTime:Math.Max(0,libvlc_media_player_get_time(player)))+delta);}
  void UpdateSeek(){if(!playing||player==IntPtr.Zero)return;duration=Math.Max(0,libvlc_media_player_get_length(player));bool enabled=duration>0&&libvlc_media_player_is_seekable(player)!=0;seek.Enabled=enabled;rewind.Enabled=enabled;forward.Enabled=enabled;if(seeking)return;long at=DateTime.UtcNow<seekDisplayUntil?requestedTime:Math.Max(0,libvlc_media_player_get_time(player));seek.Value=duration>0?(int)Math.Max(0,Math.Min(1000,1000.0*at/duration)):0;elapsed.Text=Clock(at)+" / "+Clock(duration);}
  void Back(){playing=false;Release();menu.Visible=true;menu.BringToFront();menu.Focus();}
  void Navigate(int delta){if(!playing&&menu.Items.Count>0)menu.SelectedIndex=(menu.SelectedIndex+delta+menu.Items.Count)%menu.Items.Count;}
  void Play(){if(instance==IntPtr.Zero||menu.SelectedIndex<0)return;Release();media=libvlc_media_new_path(instance,Utf8(movies[menu.SelectedIndex]));libvlc_media_add_option(media,Utf8(":no-video-title-show"));player=libvlc_media_player_new_from_media(media);libvlc_media_player_set_hwnd(player,video.Handle);libvlc_video_set_key_input(player,0);menu.Visible=false;playing=true;if(libvlc_media_player_play(player)<0){Back();MessageBox.Show(this,"This bonus feature could not be played.");}}
  protected override bool ProcessCmdKey(ref Message msg,Keys key){if(key==Keys.Left){SeekBy(-10000);return true;}if(key==Keys.Right){SeekBy(10000);return true;}if(key==Keys.Home&&playing){SeekTo(0);return true;}if(key==Keys.End&&playing){SeekTo(duration-250);return true;}if(key==Keys.Up){Navigate(-1);return true;}if(key==Keys.Down){Navigate(1);return true;}if(key==Keys.Enter){if(!playing)Play();return true;}if(key==Keys.M||key==Keys.Back){Back();return true;}if(key==Keys.Escape){if(playing)Back();else Close();return true;}if(key==Keys.Space){if(playing)libvlc_media_player_pause(player);return true;}return base.ProcessCmdKey(ref msg,key);}
  void Poll(){ticks++;if(ticks%5==0)UpdateSeek();if(playing&&probe&&ticks%125==0){uint w,h;libvlc_video_get_size(player,0,out w,out h);File.AppendAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"bonus-probe.txt"),"state="+libvlc_media_player_get_state(player)+" time="+libvlc_media_player_get_time(player)+" length="+duration+" seekable="+libvlc_media_player_is_seekable(player)+" video="+w+"x"+h+Environment.NewLine);}
   if(playing){int state=libvlc_media_player_get_state(player);if(state==6)Back();else if(state==7){Back();MessageBox.Show(this,"This bonus feature could not be decoded.","Bonus DVD");}}if(!ContainsFocus){previous=0;return;}
   for(uint user=0;user<4;user++){Pad pad;if(XInputGetState(user,out pad)!=0)continue;ushort pressed=(ushort)(pad.buttons&~previous);previous=pad.buttons;
    if((pressed&1)!=0)Navigate(-1);if((pressed&2)!=0)Navigate(1);if((pressed&4)!=0)SeekBy(-10000);if((pressed&8)!=0)SeekBy(10000);if((pressed&0x1000)!=0&&!playing)Play();if((pressed&0x2000)!=0)Back();if((pressed&0x10)!=0&&playing)libvlc_media_player_pause(player);break;
   }
  }
 }
}
