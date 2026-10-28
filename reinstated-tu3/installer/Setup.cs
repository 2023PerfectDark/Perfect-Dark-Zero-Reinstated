using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Forms=System.Windows.Forms;

namespace PdzInstaller {
 public sealed class ArtCatalog {public Dictionary<string,string> roles;public string[] comics;}
 public sealed partial class Setup : Window {
  readonly Selection choice=new Selection(); readonly Canvas scene=new Canvas{Width=1600,Height=900,Background=Brushes.Black};
  readonly ArtCatalog art; readonly bool preview; readonly int previewPage;
  StackPanel body,actions; TextBlock title,subtitle,notice,stepLabel; Border panel; Image character; Button next,back;
  Plan plan; CancellationTokenSource cancellation; bool busy; int page; string failure=""; DispatcherTimer comicTimer; int comicIndex;
  readonly MediaPlayer music=new MediaPlayer();bool muted;
  static readonly Brush Lime=new SolidColorBrush(Color.FromRgb(135,239,32)),Gold=new SolidColorBrush(Color.FromRgb(255,170,25)),Dim=new SolidColorBrush(Color.FromRgb(163,203,243));
  static readonly string[] keys={"language","welcome","game","dlc","display","dubs","bonus","shaders","space","install","complete","comics"};
  static readonly string[] steps={"Installer language","Welcome","Game & title update","Downloadable content","Display languages","Voice languages","Bonus DVD","Shader cache","Installation folder","Installing","Installation result","The comics"};
  static readonly Dictionary<string,string> names=new Dictionary<string,string>{{"english","English"},{"german","Deutsch"},{"japanese","日本語"},{"latam","Español (Latinoamérica)"},{"spanish","Español (España)"},{"french","Français"},{"italian","Italiano"},{"chinese","繁體中文"},{"korean","한국어"}};
  static readonly Dictionary<string,string> uiLanguages=new Dictionary<string,string>{{"english","en"},{"japanese","ja"},{"german","de"},{"french","fr"},{"spanish","es"},{"italian","it"},{"korean","ko"},{"chinese","zh"},{"latam","es-MX"}};
  static readonly Dictionary<string,string[]> phrases=new Dictionary<string,string[]> {
   {"Next",new[]{"Weiter","Suivant","Avanti","Siguiente","次へ"}}, {"Back",new[]{"Zurück","Retour","Indietro","Atrás","戻る"}}, {"Skip",new[]{"Überspringen","Passer","Salta","Omitir","スキップ"}}, {"Install",new[]{"Installieren","Installer","Installa","Instalar","インストール"}}, {"Finish",new[]{"Fertig","Terminer","Fine","Finalizar","終了"}}, {"Cancel",new[]{"Abbrechen","Annuler","Annulla","Cancelar","キャンセル"}},
   {"Please select a language.",new[]{"Bitte eine Sprache auswählen.","Veuillez choisir une langue.","Seleziona una lingua.","Selecciona un idioma.","言語を選択してください。"}},
   {"Welcome to Perfect Dark Zero: Reinstated!",new[]{"Willkommen bei Perfect Dark Zero: Reinstated!","Bienvenue dans Perfect Dark Zero: Reinstated !","Benvenuto in Perfect Dark Zero: Reinstated!","¡Bienvenido a Perfect Dark Zero: Reinstated!","Perfect Dark Zero: Reinstated へようこそ！"}},
   {"Add your game and optional title update.",new[]{"Spiel und optionales Titel-Update hinzufügen.","Ajoutez le jeu et sa mise à jour facultative.","Aggiungi il gioco e l'aggiornamento facoltativo.","Añade el juego y la actualización opcional.","ゲームと任意のタイトルアップデートを追加します。"}},
   {"Add your DLC, or skip this step.",new[]{"DLC hinzufügen oder diesen Schritt überspringen.","Ajoutez vos DLC ou passez cette étape.","Aggiungi i DLC o salta questo passaggio.","Añade los DLC u omite este paso.","DLC を追加するか、スキップしてください。"}},
   {"Choose additional display languages.",new[]{"Zusätzliche Textsprachen auswählen.","Choisissez les langues d'affichage supplémentaires.","Scegli le lingue aggiuntive per i testi.","Elige idiomas adicionales para los textos.","追加する表示言語を選択してください。"}},
   {"Choose the voices you want to add.",new[]{"Gewünschte Sprachausgaben auswählen.","Choisissez les doublages à ajouter.","Scegli i doppiaggi da aggiungere.","Elige los doblajes que quieres añadir.","追加する音声言語を選択してください。"}},
   {"Have the Limited Edition Bonus DVD?",new[]{"Besitzt du die Limited Edition Bonus-DVD?","Vous avez le DVD bonus de l'édition limitée ?","Hai il DVD bonus della Limited Edition?","¿Tienes el DVD extra de la edición limitada?","限定版のボーナス DVD をお持ちですか？"}},
   {"Choose where to install.",new[]{"Installationsordner auswählen.","Choisissez le dossier d'installation.","Scegli dove installare.","Elige dónde instalar.","インストール先を選択してください。"}},
   {"Installation complete!",new[]{"Installation abgeschlossen!","Installation terminée !","Installazione completata!","¡Instalación completa!","インストール完了！"}},
   {"Installation failed.",new[]{"Installation fehlgeschlagen.","Échec de l'installation.","Installazione non riuscita.","La instalación ha fallado.","インストールに失敗しました。"}},
   {"Did you know there is a Perfect Dark Zero Comic?",new[]{"Wusstest du, dass es einen Perfect Dark Zero Comic gibt?","Saviez-vous qu'il existe une BD Perfect Dark Zero ?","Sapevi che esiste un fumetto di Perfect Dark Zero?","¿Sabías que existe un cómic de Perfect Dark Zero?","Perfect Dark Zero のコミックをご存じですか？"}},
   {"Browse ISO",new[]{"ISO auswählen","Choisir l'ISO","Sfoglia ISO","Buscar ISO","ISO を選択"}}, {"Add files",new[]{"Dateien hinzufügen","Ajouter des fichiers","Aggiungi file","Añadir archivos","ファイルを追加"}}, {"Add folder",new[]{"Ordner hinzufügen","Ajouter un dossier","Aggiungi cartella","Añadir carpeta","フォルダーを追加"}}, {"Clear",new[]{"Leeren","Effacer","Cancella","Borrar","クリア"}}, {"Select all",new[]{"Alle auswählen","Tout sélectionner","Seleziona tutto","Seleccionar todo","すべて選択"}}, {"None",new[]{"Keine","Aucun","Nessuno","Ninguno","選択解除"}}, {"Check sources & space",new[]{"Quellen & Speicher prüfen","Vérifier les sources et l'espace","Verifica sorgenti e spazio","Comprobar archivos y espacio","ファイルと空き容量を確認"}},
   {"Game ISO or extracted folder",new[]{"Spiel-ISO oder entpackter Ordner","ISO du jeu ou dossier extrait","ISO del gioco o cartella estratta","ISO del juego o carpeta extraída","ゲーム ISO または展開済みフォルダー"}},
   {"Title Update 3 (optional)",new[]{"Titel-Update 3 (optional)","Mise à jour 3 (facultative)","Aggiornamento 3 (facoltativo)","Actualización 3 (opcional)","タイトルアップデート 3（任意）"}},
   {"Skip title update",new[]{"Titel-Update überspringen","Ignorer la mise à jour","Salta aggiornamento","Omitir actualización","アップデートをスキップ"}},
   {"The installer extracts your ISO automatically. Keep the original ISO; no manual extraction is needed.",new[]{"Das ISO wird automatisch entpackt. Die Originaldatei bleibt erhalten; manuelles Entpacken ist nicht nötig.","L'ISO est extraite automatiquement. Le fichier original est conservé ; aucune extraction manuelle n'est nécessaire.","L'ISO viene estratta automaticamente. Il file originale viene conservato; non serve estrarla manualmente.","El instalador extrae la ISO automáticamente y conserva el archivo original. No hace falta extraerla a mano.","ISO は自動で展開され、元のファイルは保持されます。手動での展開は不要です。"}},
   {"Installing Perfect Dark Zero: Reinstated…",new[]{"Perfect Dark Zero: Reinstated wird installiert…","Installation de Perfect Dark Zero: Reinstated…","Installazione di Perfect Dark Zero: Reinstated…","Instalando Perfect Dark Zero: Reinstated…","Perfect Dark Zero: Reinstated をインストール中…"}}
  };
  Dictionary<string,string[]> extra=new Dictionary<string,string[]>();
  string T(string s){string[] a;string language=choice.UiLanguage=="es-MX"?"es":choice.UiLanguage;int i=Array.IndexOf(new[]{"de","fr","it","es","ja","zh","ko"},language);return i>=0&&(extra.TryGetValue(s,out a)||phrases.TryGetValue(s,out a))&&i<a.Length&&!String.IsNullOrEmpty(a[i])?a[i]:s;}
  [STAThread] public static void Main(string[] args){try{var app=new Application();app.Run(new Setup(args));}catch(Exception e){MessageBox.Show(e.Message,"PDZ Setup",MessageBoxButton.OK,MessageBoxImage.Error);}}
  public Setup(string[] args){preview=args.Any(a=>a.StartsWith("--preview="));previewPage=preview?Int32.Parse(args.First(a=>a.StartsWith("--preview=")).Split('=')[1]):0;
   art=new JavaScriptSerializer().Deserialize<ArtCatalog>(File.ReadAllText(Path.Combine(Engine.Root,"art.json")));string locale=Path.Combine(Engine.Root,"text.json");if(File.Exists(locale))extra=new JavaScriptSerializer().Deserialize<Dictionary<string,string[]>>(File.ReadAllText(locale));
   Title="Perfect Dark Zero: Reinstated — d-PAL Installer";Width=1280;Height=760;MinWidth=960;MinHeight=620;Background=Brushes.Black;WindowStartupLocation=WindowStartupLocation.CenterScreen;
   Content=new Viewbox{Stretch=Stretch.Uniform,Child=scene};FontFamily=new FontFamily("Segoe UI");scene.LayoutUpdated+=(s,e)=>{if(actions!=null)Canvas.SetLeft(actions,1440-actions.ActualWidth);};
   Resources.Add(typeof(Button),ButtonStyle());
   choice.Target=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Perfect Dark Zero Reinstated");
   choice.Dubs.Add("english");choice.TextLanguages.Add("english");
   Closing+=OnClosing;PreviewKeyDown+=(s,e)=>{if(transitioning){if(e.Key==Key.Escape||e.Key==Key.Enter||e.Key==Key.Space)e.Handled=true;return;}if(page==11&&e.Key==Key.Space){PlayCue("select");SetComicsPaused(!comicsPaused);e.Handled=true;return;}if(e.Key==Key.Escape&&!busy){PlayCue("back");if(page>0&&page<9){InvalidatePlan();Show(page-1);}else if(page==0||page>=10)Close();e.Handled=true;}};
   InitDpal(args);Show(previewPage);string song=Path.Combine(Engine.Root,"art/mission-select.wav");if(File.Exists(song)){music.Volume=.27;music.MediaEnded+=(s,e)=>{music.Position=TimeSpan.FromSeconds(0);music.Play();};music.Open(new Uri(song));music.Play();}Closed+=(s,e)=>music.Close();
  }
  static Style ButtonStyle(){var s=new Style(typeof(Button));s.Setters.Add(new Setter(Control.ForegroundProperty,Dim));s.Setters.Add(new Setter(Control.BackgroundProperty,new SolidColorBrush(Color.FromRgb(9,57,115))));s.Setters.Add(new Setter(Control.BorderBrushProperty,new SolidColorBrush(Color.FromRgb(32,127,219))));s.Setters.Add(new Setter(Control.BorderThicknessProperty,new Thickness(1)));s.Setters.Add(new Setter(Control.FontFamilyProperty,new FontFamily("Bahnschrift")));s.Setters.Add(new Setter(Control.FontWeightProperty,FontWeights.Bold));s.Setters.Add(new Setter(Control.FontSizeProperty,21.0));s.Setters.Add(new Setter(Control.PaddingProperty,new Thickness(20,9,20,9)));s.Setters.Add(new Setter(FrameworkElement.MarginProperty,new Thickness(0,6,10,6)));s.Setters.Add(new Setter(Control.CursorProperty,Cursors.Hand));
   var template=new ControlTemplate(typeof(Button));var border=new FrameworkElementFactory(typeof(Border));border.SetBinding(Border.BackgroundProperty,new System.Windows.Data.Binding("Background"){RelativeSource=new System.Windows.Data.RelativeSource(System.Windows.Data.RelativeSourceMode.TemplatedParent)});border.SetBinding(Border.BorderBrushProperty,new System.Windows.Data.Binding("BorderBrush"){RelativeSource=new System.Windows.Data.RelativeSource(System.Windows.Data.RelativeSourceMode.TemplatedParent)});border.SetValue(Border.BorderThicknessProperty,new Thickness(1));var cp=new FrameworkElementFactory(typeof(ContentPresenter));cp.SetValue(FrameworkElement.MarginProperty,new Thickness(18,9,18,9));cp.SetValue(FrameworkElement.HorizontalAlignmentProperty,HorizontalAlignment.Center);cp.SetValue(FrameworkElement.VerticalAlignmentProperty,VerticalAlignment.Center);border.AppendChild(cp);template.VisualTree=border;s.Setters.Add(new Setter(Control.TemplateProperty,template));
   var hover=new Trigger{Property=UIElement.IsMouseOverProperty,Value=true};hover.Setters.Add(new Setter(Control.BackgroundProperty,new SolidColorBrush(Color.FromRgb(19,103,194))));hover.Setters.Add(new Setter(Control.BorderBrushProperty,Gold));hover.Setters.Add(new Setter(Control.ForegroundProperty,Gold));s.Triggers.Add(hover);
   var focus=new Trigger{Property=UIElement.IsKeyboardFocusedProperty,Value=true};focus.Setters.Add(new Setter(Control.BorderBrushProperty,Gold));focus.Setters.Add(new Setter(Control.ForegroundProperty,Gold));focus.Setters.Add(new Setter(Control.BackgroundProperty,new SolidColorBrush(Color.FromRgb(19,103,194))));s.Triggers.Add(focus);
   var disabled=new Trigger{Property=UIElement.IsEnabledProperty,Value=false};disabled.Setters.Add(new Setter(UIElement.OpacityProperty,.4));s.Triggers.Add(disabled);return s;
  }
  static DrawingBrush Lines(Color c,int spacing){var g=new DrawingGroup();g.Children.Add(new GeometryDrawing(Brushes.Transparent,null,new RectangleGeometry(new Rect(0,0,spacing,spacing))));g.Children.Add(new GeometryDrawing(new SolidColorBrush(c),null,new RectangleGeometry(new Rect(0,0,spacing,1))));return new DrawingBrush(g){TileMode=TileMode.Tile,ViewportUnits=BrushMappingMode.Absolute,Viewport=new Rect(0,0,spacing,spacing)};}
  void At(UIElement e,double x,double y){Place(pageLayer,e,x,y);}
  TextBlock Text(string s,double size,Brush color=null){return new TextBlock{Text=T(s),FontSize=size,Foreground=color??Brushes.White,TextWrapping=TextWrapping.Wrap,Margin=new Thickness(0,0,0,13),FontWeight=FontWeights.Normal};}
  Button Button(string s,Action action){var b=new Button{Content=T(s).ToUpperInvariant()};TrackCursor(b);b.Click+=(o,e)=>{if(transitioning)return;PlayCue(b==next&&(page==9||String.Equals(b.Content as string,T("Skip").ToUpperInvariant()))?"back":ActionCue(s));try{action();}catch(Exception ex){Error(ex.Message);}};return b;}
  void Note(string s){body.Children.Add(Text(s,20,Dim));}
  void Error(string s){notice.Text=s;notice.Foreground=new SolidColorBrush(Color.FromRgb(255,159,123));PlayCue("error");}
  StackPanel Row(params UIElement[] children){var r=new StackPanel{Orientation=Orientation.Horizontal};foreach(var c in children)r.Children.Add(c);return r;}
  void InvalidatePlan(){if(plan!=null){plan.Dispose();plan=null;}if(page==8&&next!=null)next.IsEnabled=false;}
  void Show(int value){StopWelcome();if(comicTimer!=null){comicTimer.Stop();comicTimer=null;}if(value==3&&rev1Source&&!preview)value=page>3?2:4;page=value;UpdateChromeLanguage();Canvas old=BeginPage(value);
   var heading=Text(T("Installer").ToUpperInvariant()+"  |  "+T(steps[page]).ToUpperInvariant(),28,Gold);heading.FontFamily=new FontFamily("Bahnschrift");heading.FontWeight=FontWeights.SemiBold;heading.Width=1260;heading.TextAlignment=TextAlignment.Right;At(heading,160,110);
   stepLabel=Text((preview?"PREVIEW  •  ":"")+String.Format("{0:00} / {1:00}",page+1,keys.Length),18,Dim);At(stepLabel,164,168);
   var marks=new StackPanel{Orientation=Orientation.Horizontal};for(int i=0;i<keys.Length;i++)marks.Children.Add(new Border{Width=47,Height=5,Margin=new Thickness(0,0,7,0),Background=i==page?Gold:i<page?Blue:Ink});At(marks,778,177);
   string path;if(page!=11&&art.roles.TryGetValue(keys[page],out path)){character=new Image{Source=GamePortrait(path),Width=430,Height=600,Stretch=Stretch.Uniform};At(character,155,208);}
   panel=new Border{Width=830,Height=522,Padding=new Thickness(25,22,25,12),Background=new SolidColorBrush(Color.FromArgb(215,4,20,49)),BorderBrush=new SolidColorBrush(Color.FromRgb(29,110,196)),BorderThickness=new Thickness(1)};At(panel,610,215);
   var grid=new Grid();panel.Child=grid;grid.Children.Add(new Border{Background=Lines(Color.FromArgb(40,0,0,0),4),IsHitTestVisible=false});
   grid.RowDefinitions.Add(new RowDefinition{Height=GridLength.Auto});grid.RowDefinitions.Add(new RowDefinition{Height=new GridLength(1,GridUnitType.Star)});grid.RowDefinitions.Add(new RowDefinition{Height=GridLength.Auto});
   title=Text("",32,Gold);grid.Children.Add(title);body=new StackPanel();bodyScroll=new ScrollViewer{Content=body,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};Grid.SetRow(bodyScroll,1);grid.Children.Add(bodyScroll);
   notice=Text("",18,Dim);notice.MaxHeight=65;Grid.SetRow(notice,2);grid.Children.Add(notice);
   actions=new StackPanel{Orientation=Orientation.Horizontal,HorizontalAlignment=HorizontalAlignment.Right};At(actions,610,752);
   back=Button("Back",()=>{InvalidatePlan();Show(page-1);});back.Visibility=page>0&&page<9?Visibility.Visible:Visibility.Collapsed;actions.Children.Add(back);
   next=Button("Next",()=>Next());next.MinWidth=150;next.IsDefault=true;actions.Children.Add(next);
   if(page==0)LanguagePage();else if(page==1)Welcome();else if(page==2)GamePage();else if(page==3)DlcPage();else if(page==4||page==5)LanguagesPage(page==5);else if(page==6)BonusPage();else if(page==7)ShadersPage();else if(page==8)SpacePage();else if(page==9)ProgressPage();else if(page==10)ResultPage();else ComicsPage();
   FinishPage(old,value);
  }
  BitmapImage Load(string rel){var b=new BitmapImage();b.BeginInit();b.CacheOption=BitmapCacheOption.OnLoad;b.UriSource=new Uri(Path.Combine(Engine.Root,rel));b.EndInit();b.Freeze();return b;}
  BitmapSource GamePortrait(string rel){
   var original=Load(rel);
   var pixels=new FormatConvertedBitmap(original,PixelFormats.Bgra32,null,0);
   int w=pixels.PixelWidth,h=pixels.PixelHeight,stride=w*4;
   var data=new byte[stride*h];pixels.CopyPixels(data,stride,0);
   int left=w,top=h,right=-1,bottom=-1;
   // Ignore nearly invisible alpha specks when fitting the portrait to its slot.
   for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(data[y*stride+x*4+3]>=8){left=Math.Min(left,x);top=Math.Min(top,y);right=Math.Max(right,x);bottom=Math.Max(bottom,y);}
   if(right<left)return original;
   left=Math.Max(0,left-8);top=Math.Max(0,top-8);right=Math.Min(w-1,right+8);bottom=Math.Min(h-1,bottom+8);
   var portrait=new CroppedBitmap(original,new Int32Rect(left,top,right-left+1,bottom-top+1));portrait.Freeze();return portrait;
  }
  void Next(){if(busy||page==9&&!preview)return;if(preview){Show((page+1)%keys.Length);return;}if(page==2&&String.IsNullOrWhiteSpace(choice.Game)){Error(T("Select your US original or Rev 1 game ISO or extracted folder."));return;}
   if(page==2){try{using(var disc=new Disc(choice.Game)){disc.ValidateRetail();rev1Source=disc.IsRev1;if(rev1Source){choice.Update="";choice.Dlc.Clear();}}}catch(Exception e){Error(e.Message);return;}}
   if(page==8){if(plan!=null)Install();return;}if(page==10&&failure!=""){Show(8);return;}if(page==11){Close();return;}Show(page+1);
  }
  void LanguagePage(){title.Text=T("Please select a language.");Note("Select the language used by this installer. Game text and voices are chosen separately.");var g=new System.Windows.Controls.Primitives.UniformGrid{Columns=2,Margin=new Thickness(0,12,0,0)};
   foreach(string language in Engine.TextIds.Keys){string code=uiLanguages[language];var b=Button((choice.UiLanguage==code?"■  ":"□  ")+names[language],()=>{choice.UiLanguage=code;Show(0);});b.Height=52;b.FontSize=19;b.Margin=new Thickness(0,3,10,3);g.Children.Add(b);}body.Children.Add(g);
  }

  string PickFile(string filter,bool multiple,List<string> append=null){var d=new Microsoft.Win32.OpenFileDialog{Filter=filter,Multiselect=multiple,CheckFileExists=true};if(d.ShowDialog(this)!=true){PlayCue("back");return null;}PlayCue("select");if(append!=null)foreach(var f in d.FileNames)if(!append.Contains(f))append.Add(f);return d.FileName;}
  string PickFolder(){using(var d=new Forms.FolderBrowserDialog{Description=T("Choose a folder"),ShowNewFolderButton=true}){bool ok=d.ShowDialog()==Forms.DialogResult.OK;PlayCue(ok?"select":"back");return ok?d.SelectedPath:null;}}
  void Source(string label,string value,Action<string> set,string filter,bool folders){body.Children.Add(Text(label,22,Lime));var box=new TextBox{Text=value,FontSize=18,Padding=new Thickness(8),Background=new SolidColorBrush(Color.FromRgb(3,15,35)),Foreground=Brushes.White,BorderBrush=Dim,Margin=new Thickness(0,0,10,3)};box.TextChanged+=(s,e)=>{set(box.Text.Trim());InvalidatePlan();};body.Children.Add(box);
   var r=Row(Button("Browse ISO",()=>{var f=PickFile(filter,false);if(f!=null)box.Text=f;}));if(folders)r.Children.Add(Button("Add folder",()=>{var f=PickFolder();if(f!=null)box.Text=f;}));body.Children.Add(r);
  }
  bool rev1Source;
  StackPanel updateControls;
  TextBlock revisionNote;
  void DetectSourceRevision(){
   rev1Source=false;
   try{if(File.Exists(choice.Game)||Directory.Exists(choice.Game))using(var disc=new Disc(choice.Game)){disc.ValidateRetail();rev1Source=disc.IsRev1;}}catch{ }
   if(rev1Source){choice.Update="";choice.Dlc.Clear();}
   if(updateControls!=null)updateControls.IsEnabled=!rev1Source;
   if(revisionNote!=null)revisionNote.Text=T(rev1Source?"US Rev 1 detected. Disc DLC is included automatically; the DLC step is skipped. External Title Update 3 is disabled. Reinstated compatibility is prepared automatically.":"Supported: original US ISO and US Rev 1 ISO. Original discs may use optional Title Update 3 and separate DLC.");
  }
  bool UsesTu3Source {get{return rev1Source||choice.Update!="";}}
  void GamePage(){title.Text=T("Add your game and optional title update.");Source("Game ISO or extracted folder",choice.Game,s=>{choice.Game=s;DetectSourceRevision();},"Xbox 360 disc image|*.iso|All files|*.*",true);
   Note("The installer extracts your ISO automatically. Keep the original ISO; no manual extraction is needed.");
   body.Children.Add(Text("Title Update 3 (optional)",22,Lime));var row=Row(Button("Add files",()=>{DetectSourceRevision();if(rev1Source)return;var f=PickFile("Title update or ZIP|*.zip;TU_*|All files|*.*",false);if(f!=null){choice.Update=f;Show(2);}}),Button("Skip title update",()=>{choice.Update="";Show(2);}));updateControls=row;body.Children.Add(row);Note(choice.Update==""?"Title update: skipped":Path.GetFileName(choice.Update));revisionNote=Text("",18,Dim);body.Children.Add(revisionNote);DetectSourceRevision();}
  void SourceList(List<string> values){foreach(var path in values.ToArray()){string p=path;var r=new DockPanel{Margin=new Thickness(0,0,0,3)};var remove=Button("×",()=>{values.Remove(p);InvalidatePlan();Show(page);});DockPanel.SetDock(remove,Dock.Right);r.Children.Add(remove);r.Children.Add(Text(Path.GetFileName(p),18,Dim));body.Children.Add(r);}}
  void DlcPage(){if(rev1Source)Note("Rev 1 includes both map packs on disc. Separate DLC packages are not required.");title.Text=T("Add your DLC, or skip this step.");Note("Add Map Pack One and/or Map Pack Two from your own Xbox 360 content. Select the content packages, ZIP files or folders containing them.");body.Children.Add(Row(Button("Add files",()=>{PickFile("DLC packages and ZIP|*.*",true,choice.Dlc);Show(3);}),Button("Add folder",()=>{var f=PickFolder();if(f!=null){choice.Dlc.Add(f);Show(3);}}),Button("Clear",()=>{choice.Dlc.Clear();Show(3);})));SourceList(choice.Dlc);next.Content=T(choice.Dlc.Count==0?"Skip":"Next").ToUpperInvariant();}
  void LanguagesPage(bool dub){title.Text=T(dub?"Choose the voices you want to add.":"Choose additional display languages.");var chosen=dub?choice.Dubs:choice.TextLanguages;var list=dub?Engine.DubNames:Engine.TextIds.Keys.ToArray();
   Note(dub?"Select one or more voice languages, or all of them. English, French and Latin American Spanish are on the US disc.":"Keep English or add other display languages, including Japanese, Chinese and Korean to switch to in game");
   var g=new System.Windows.Controls.Primitives.UniformGrid{Columns=3};foreach(var l in list){string lang=l;var box=new CheckBox{Content=names[l],IsChecked=chosen.Contains(l),Foreground=Lime,FontSize=18,Margin=new Thickness(0,5,8,10)};TrackCursor(box);box.Checked+=(s,e)=>{PlayCue("checkbox");if(!chosen.Contains(lang))chosen.Add(lang);};box.Unchecked+=(s,e)=>{PlayCue("checkbox");chosen.Remove(lang);};g.Children.Add(box);}body.Children.Add(g);
   body.Children.Add(Row(Button("Select all",()=>{chosen.Clear();chosen.AddRange(list);Show(page);}),Button("None",()=>{chosen.Clear();Show(page);})));body.Children.Add(Text(dub?"Default voice language":"Default display language",18,Dim));
   var combo=new ComboBox{FontSize=18,MinWidth=300,HorizontalAlignment=HorizontalAlignment.Left,Margin=new Thickness(0,0,0,15),ItemsSource=list.Select(l=>names[l]).ToArray(),SelectedIndex=Array.IndexOf(list,dub?choice.Dub:choice.TextLanguage)};TrackCursor(combo);combo.SelectionChanged+=(s,e)=>{if(combo.SelectedIndex>=0){PlayCue("select");string l=list[combo.SelectedIndex];if(dub)choice.Dub=l;else choice.TextLanguage=l;if(!chosen.Contains(l))chosen.Add(l);}};body.Children.Add(combo);
   Note("Regional sources: Europe (En,Fr,De), Europe (En,Es,It), or Japan/Korea (En,Ja,Zh,Ko). Add the matching Perfect Dark Zero ISO or extracted folder.");
   body.Children.Add(Row(Button("Browse ISO",()=>{PickFile("Xbox 360 disc image|*.iso",true,choice.LanguageSources);Show(page);}),Button("Add folder",()=>{var f=PickFolder();if(f!=null){choice.LanguageSources.Add(f);Show(page);}})));SourceList(choice.LanguageSources);
   body.Children.Add(Button("Skip",()=>{chosen.Clear();if(dub)choice.Dub="english";else choice.TextLanguage="english";Show(page+1);}));if(chosen.Count==0)next.Content=T("Skip").ToUpperInvariant();
  }
  void BonusPage(){title.Text=T("Have the Limited Edition Bonus DVD?");Source("Perfect Dark Zero Bonus DVD ISO or ZIP",choice.Bonus,s=>choice.Bonus=s,"DVD image or ZIP|*.iso;*.zip",false);Note("This is the separate Bonus DVD, not the Limited Edition game disc. You can skip it.");Note("Watch the bonus videos in the included PC viewer. Xbox controller: D-pad to move, A to play, B to return, Start to pause. Keyboard controls are also available.");Note("The original Xbox 360 bonus menu is not ported. The disc's files are preserved, and its videos are available in the PC viewer.");body.Children.Add(Button("Clear",()=>{choice.Bonus="";Show(6);}));next.Content=T(choice.Bonus==""?"Skip":"Next").ToUpperInvariant();}
  void ShadersPage(){
   title.Text=T("Choose your shader cache.");
   Note("Use the completed Reinstated playthrough cache, or start fresh and build your own as you play.");
   bool available=UsesTu3Source||preview;
   var shared=new RadioButton{GroupName="shader-cache",Content=T("Use the completed playthrough cache (recommended)"),IsChecked=available&&choice.UseSharedShaders,IsEnabled=available,Foreground=Lime,FontSize=23,Margin=new Thickness(0,12,0,10)};
   var own=new RadioButton{GroupName="shader-cache",Content=T("Build my own cache"),IsChecked=!available||!choice.UseSharedShaders,Foreground=Lime,FontSize=23,Margin=new Thickness(0,14,0,10)};
   TrackCursor(shared);TrackCursor(own);
   shared.Checked+=(s,e)=>{choice.UseSharedShaders=true;InvalidatePlan();PlayCue("select");};
   own.Checked+=(s,e)=>{choice.UseSharedShaders=false;InvalidatePlan();PlayCue("select");};
   body.Children.Add(shared);
   Note("Included with this installer. Prepares known shaders before play to help reduce in-game compilation pauses. Your PC still compiles them for its GPU; new effects may still need shaders.");
   body.Children.Add(own);
   Note("Starts with an empty cache. Shaders are saved as you play; first-time effects may cause pauses or pop in.");
   if(!available)Note("The shared cache requires Rev 1 or an original disc with Title Update 3. Go back to Game & title update to change your source, or build your own cache.");
  }
  void SpacePage(){title.Text=T("Choose where to install.");Note("Choose a new folder for Perfect Dark Zero: Reinstated.");var box=new TextBox{Text=choice.Target,FontSize=20,Padding=new Thickness(10),Margin=new Thickness(0,5,0,12)};box.TextChanged+=(s,e)=>{choice.Target=box.Text.Trim();InvalidatePlan();};body.Children.Add(box);body.Children.Add(Row(Button("Add folder",()=>{var f=PickFolder();if(f!=null)box.Text=Path.Combine(f,"Perfect Dark Zero Reinstated");}),Button("Check sources & space",()=>Check())));Note("The space check includes the recomp mod, game files, selected add-ons and languages, plus the Bonus DVD if selected.");Note(UsesTu3Source&&choice.UseSharedShaders?"Shader cache: completed playthrough cache":"Shader cache: build my own");subtitle=Text("",23,Dim);body.Children.Add(subtitle);next.Content=T("Install").ToUpperInvariant();next.IsEnabled=preview;}
  async void Check(){if(busy)return;busy=true;cancellation=new CancellationTokenSource();actions.IsEnabled=false;body.IsEnabled=false;notice.Text=T("Checking your game, add-ons and available disk space…");try{InvalidatePlan();if(choice.TextLanguages.Count==0)choice.TextLanguage="english";if(choice.Dubs.Count==0)choice.Dub="english";
    if(!choice.TextLanguages.Contains(choice.TextLanguage))choice.TextLanguages.Add(choice.TextLanguage);if(!choice.Dubs.Contains(choice.Dub))choice.Dubs.Add(choice.Dub);
    plan=await Task.Run(()=>Engine.Prepare(choice,cancellation.Token));subtitle.Text=T("Recomp mod")+": "+Safe.Size(plan.ModBytes)+"\n"+T("Total installation")+": "+Safe.Size(plan.InstallBytes)+"\n"+T("Free disk space")+": "+Safe.Size(plan.Free);notice.Text=T("Ready to install.");notice.Foreground=Dim;next.IsEnabled=true;
   }catch(Exception e){Error(e.Message);}finally{cancellation.Dispose();cancellation=null;busy=false;body.IsEnabled=true;actions.IsEnabled=true;}}
  ProgressBar bar;TextBlock progressDetail,estimate;void ProgressPage(){title.Text=T("Installing Perfect Dark Zero: Reinstated…");subtitle=Text("Preparing…",26,Lime);body.Children.Add(subtitle);bar=new ProgressBar{Height=23,Minimum=0,Maximum=100,Foreground=Lime,Background=Brushes.Black,Margin=new Thickness(0,30,0,22)};body.Children.Add(bar);progressDetail=Text("",18,Dim);body.Children.Add(progressDetail);estimate=Text("Estimating time remaining…",23);body.Children.Add(estimate);next.Content=T("Cancel").ToUpperInvariant();next.Click+=(s,e)=>{if(cancellation!=null){cancellation.Cancel();next.IsEnabled=false;notice.Text=T("Cancelling after the current file operation…");}};
   if(preview){bar.Value=64;subtitle.Text=T("Installing and verifying the recomp mod");progressDetail.Text="64%   •   Preview";estimate.Text=T("Time remaining appears here during installation.");}}
  async void Install(){busy=true;cancellation=new CancellationTokenSource();Show(9);try{var p=new Progress<Status>(s=>{subtitle.Text=T(s.Stage);bar.Value=s.Total>0?Math.Min(100,100.0*s.Done/s.Total):0;progressDetail.Text=String.Format("{0:0}%  •  {1} / {2}\n{3}",bar.Value,Safe.Size(s.Done),Safe.Size(s.Total),s.Detail);double remaining=s.Done>0?s.Seconds*(s.Total-s.Done)/s.Done:0;estimate.Text=s.Seconds>4&&s.Done>0?T("Estimated time remaining")+": "+TimeSpan.FromSeconds(Math.Max(0,remaining)).ToString(@"hh\:mm\:ss"):T("Estimating time remaining…");});await Task.Run(()=>Engine.Install(plan,p,cancellation.Token));failure="";}catch(OperationCanceledException){failure="Installation cancelled.";}catch(Exception ex){failure=ex.Message;}finally{plan.Dispose();plan=null;cancellation.Dispose();cancellation=null;busy=false;}Show(10);}
  void ResultPage(){bool ok=failure=="";title.Text=T(ok?"Installation complete!":"Installation failed.");body.Children.Add(Text(ok?(preview?"Your new installation is ready. (Preview)":choice.Target):failure,25,ok?Dim:new SolidColorBrush(Color.FromRgb(255,159,123))));
   if(ok){Note("Your game is ready. Open the game EXE in your installation folder.");body.Children.Add(Button("Play Perfect Dark Zero",()=>{if(!preview)Process.Start(new ProcessStartInfo(Path.Combine(choice.Target,File.Exists(Path.Combine(choice.Target,"PDZReinstatedTU3.exe"))?"PDZReinstatedTU3.exe":"perfectdarkzerorecomp.exe")){UseShellExecute=true,WorkingDirectory=choice.Target});}));if(choice.Bonus!="")body.Children.Add(Button("Play Bonus DVD",()=>{if(!preview)Process.Start(Path.Combine(choice.Target,"PDZ Bonus DVD.exe"));}));body.Children.Add(Button("Open installation folder",()=>{if(!preview)Process.Start("explorer.exe",Safe.Quote(choice.Target));}));next.Content=T("Next").ToUpperInvariant();}else{Note("The source files were kept. Check the message above, then return to the space check to try again.");next.Content=T("Try again").ToUpperInvariant();}}

  void OnClosing(object sender,CancelEventArgs e){if(busy){e.Cancel=true;if(cancellation!=null){cancellation.Cancel();notice.Text=T("Cancelling installation…");}else notice.Text=T("Please wait for the source check to finish.");return;}StopWelcome();if(plan!=null)plan.Dispose();if(comicTimer!=null)comicTimer.Stop();}
 }
}
