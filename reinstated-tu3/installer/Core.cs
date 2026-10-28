using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

namespace PdzInstaller {
 public sealed class PayloadFile {public string source,destination,group,sha256; public long size;}
 public sealed class PayloadManifest {public string version;public PayloadFile[] files;}
 public sealed class Entry {public string Name;public long Offset,Size;}
 public static class Safe {
  public static string Child(string root,string name) {
   if(String.IsNullOrWhiteSpace(name)||Path.IsPathRooted(name)||name.Contains(":"))throw new InvalidDataException("Unsafe file name: "+name);
   var parts=name.Replace('\\','/').Split('/');
   if(parts.Any(p=>p==".."||p=="."||p.Length==0||p.EndsWith(".")||p.EndsWith(" ")||p.IndexOfAny(Path.GetInvalidFileNameChars())>=0||System.Text.RegularExpressions.Regex.IsMatch(p,@"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\.)",System.Text.RegularExpressions.RegexOptions.IgnoreCase)))throw new InvalidDataException("Unsafe file name: "+name);
   var full=Path.GetFullPath(Path.Combine(root,name));var prefix=Path.GetFullPath(root).TrimEnd('\\')+"\\";
   if(!full.StartsWith(prefix,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("File leaves installation folder.");return full;
  }
  public static byte[] Read(Stream stream,long offset,int count) {if(offset<0||count<0||offset>stream.Length-count)throw new InvalidDataException("Truncated source file.");stream.Position=offset;var b=new byte[count];int n=0,k;while(n<count&&(k=stream.Read(b,n,count-n))>0)n+=k;if(n!=count)throw new EndOfStreamException();return b;}
  public static uint BE(byte[] b,int p){return ((uint)b[p]<<24)|((uint)b[p+1]<<16)|((uint)b[p+2]<<8)|b[p+3];}
  public static int LE24(byte[] b,int p){return b[p]|b[p+1]<<8|b[p+2]<<16;}
  public static string Hash(Stream s){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(s)).Replace("-","").ToLowerInvariant();}
  public static string Hash(string p){using(var s=File.OpenRead(p))return Hash(s);}
  public static string Hash(byte[] b){using(var s=new MemoryStream(b))return Hash(s);}
  public static string Quote(string s){return "\""+s.Replace("\"","")+"\"";}
  public static string Size(long n){return (n/1073741824.0).ToString("0.00")+" GB";}
  public static void Write(string p,string s){Directory.CreateDirectory(Path.GetDirectoryName(p));File.WriteAllText(p,s,new UTF8Encoding(false));}
  public static IEnumerable<string> Files(string path) {
   foreach(var f in Directory.GetFiles(path)){if((File.GetAttributes(f)&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked source files are not supported.");yield return f;}
   foreach(var d in Directory.GetDirectories(path)){if((File.GetAttributes(d)&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked source folders are not supported.");foreach(var f in Files(d))yield return f;}
  }
 }
 public sealed class Disc : IDisposable {
  public const string RetailHash="e50192f437bd1ff70a65fae8b5561aa822ab91052fe35fd500594689e130859e";
  public const string Rev1Hash="878618dcc302e4662596fea699e17248b0b1638637c3c64c05e42f03f9390e17";
  public bool IsRev1 {get;private set;}
  public string ExecutableHash {get{return IsRev1?Rev1Hash:RetailHash;}}
  public readonly string PathName;public readonly bool IsFolder;public readonly List<Entry> Entries=new List<Entry>();
  FileStream stream;long partition;HashSet<long> directories=new HashSet<long>();
  public Disc(string path) {
   PathName=Path.GetFullPath(path);IsFolder=Directory.Exists(path);
   if(IsFolder){foreach(var p in Safe.Files(PathName)){var name=p.Substring(PathName.TrimEnd('\\').Length+1).Replace('\\','/');Safe.Child(PathName,name);Entries.Add(new Entry{Name=name,Size=new FileInfo(p).Length});}return;}
   stream=File.OpenRead(path);byte[] header=null;
   foreach(long start in new long[]{0,0xFD90000,0x2080000}){if(stream.Length<start+0x10800)continue;var h=Safe.Read(stream,start+0x10000,2048);if(Encoding.ASCII.GetString(h,0,20)=="MICROSOFT*XBOX*MEDIA"){partition=start;header=h;break;}}
   if(header==null){Dispose();throw new InvalidDataException("This is not a supported Xbox 360 game ISO.");}
   try {Walk(BitConverter.ToUInt32(header,20),BitConverter.ToUInt32(header,24),"",0);if(Entries.Select(e=>e.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count()!=Entries.Count)throw new InvalidDataException("Duplicate disc paths.");}catch{Dispose();throw;}
  }
  void Walk(uint sector,uint length,string prefix,int depth) {
   if(length==0)return;if(depth>32||length>16*1024*1024||!directories.Add(sector))throw new InvalidDataException("Invalid disc directory.");
   if(length==0)return;var data=Safe.Read(stream,partition+(long)sector*2048,(int)length);var seen=new HashSet<int>();var nodes=new Stack<int>();nodes.Push(0);
   while(nodes.Count>0){int o=nodes.Pop();if(o<0||o+14>data.Length||!seen.Add(o))throw new InvalidDataException("Invalid disc entry.");int left=BitConverter.ToUInt16(data,o)*4,right=BitConverter.ToUInt16(data,o+2)*4,n=data[o+13];
    if(n==0||o+14+n>data.Length)throw new InvalidDataException("Invalid disc file name.");string leaf=Encoding.ASCII.GetString(data,o+14,n);string name=prefix+leaf;Safe.Child(System.IO.Path.GetTempPath(),name);
    uint s=BitConverter.ToUInt32(data,o+4),size=BitConverter.ToUInt32(data,o+8);
    if((data[o+12]&16)!=0)Walk(s,size,name+"/",depth+1);else{long offset=partition+(long)s*2048;if(offset>stream.Length-size)throw new InvalidDataException("Disc file is truncated.");Entries.Add(new Entry{Name=name,Offset=offset,Size=size});}
    if(left!=0)nodes.Push(left);if(right!=0)nodes.Push(right);if(Entries.Count>100000)throw new InvalidDataException("Too many disc files.");
   }
  }
  public byte[] Bytes(Entry e){if(e.Size>16*1024*1024)throw new InvalidDataException("Unexpectedly large metadata file.");return IsFolder?File.ReadAllBytes(Safe.Child(PathName,e.Name)):Safe.Read(stream,e.Offset,(int)e.Size);}
  public void ValidateRetail(){var e=Entries.SingleOrDefault(x=>x.Name.Equals("default.xex",StringComparison.OrdinalIgnoreCase));string hash=e==null?"":Safe.Hash(Bytes(e));
   IsRev1=hash==Rev1Hash;if(hash!=RetailHash&&!IsRev1)throw new InvalidDataException("Select a clean US original or US Rev 1 Perfect Dark Zero ISO or extracted folder.");
   string index=Path.Combine(Engine.Root,IsRev1?"rev1-index.json":"retail-index.json");if(!File.Exists(index))throw new IOException("The installer's disc file index is missing.");var expected=new JavaScriptSerializer().Deserialize<Entry[]>(File.ReadAllText(index));var found=Entries.ToDictionary(x=>x.Name,StringComparer.OrdinalIgnoreCase);foreach(var f in expected){Entry actual;if(!found.TryGetValue(f.Name,out actual)||actual.Size!=f.Size)throw new InvalidDataException("The game source is incomplete or modified: "+f.Name+". Select a clean US ISO or extraction.");}
   if(IsRev1&&Entries.Any(x=>x.Name.Equals("default.xexp",StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("Remove the separately added title-update patch from this Rev 1 extraction, or select the clean Rev 1 ISO.");}
  public void ValidateLanguage(){var e=Entries.SingleOrDefault(x=>x.Name.Equals("default.xex",StringComparison.OrdinalIgnoreCase));if(e==null)throw new InvalidDataException("No game executable found on this language disc.");var b=Bytes(e);if(b.Length<24||Encoding.ASCII.GetString(b,0,4)!="XEX2")throw new InvalidDataException("Invalid language disc.");uint count=Safe.BE(b,20);bool match=false;for(uint i=0;i<count&&24+i*8+8<=b.Length;i++){int off=(int)(24+i*8);if(Safe.BE(b,off)==0x40006){uint p=Safe.BE(b,off+4);if(p+16<=b.Length&&Safe.BE(b,(int)p+12)==0x4D5307D3)match=true;}}if(!match)throw new InvalidDataException("This language source is not Perfect Dark Zero.");}
  public void Copy(Entry e,string output,Action<long> progress,CancellationToken token){Directory.CreateDirectory(System.IO.Path.GetDirectoryName(output));using(var dest=new FileStream(output,FileMode.CreateNew,FileAccess.Write)){if(IsFolder){using(var input=File.OpenRead(Safe.Child(PathName,e.Name)))CopyStream(input,dest,e.Size,progress,token);}else{stream.Position=e.Offset;CopyStream(stream,dest,e.Size,progress,token);}}}
  public static void CopyStream(Stream src,Stream dest,long length,Action<long> progress,CancellationToken token){var b=new byte[1024*1024];long remaining=length;while(remaining>0){token.ThrowIfCancellationRequested();int n=src.Read(b,0,(int)Math.Min(remaining,b.Length));if(n==0)throw new EndOfStreamException();dest.Write(b,0,n);remaining-=n;progress(n);}}
  public void Dispose(){if(stream!=null)stream.Dispose();}
 }
 public sealed class Stfs : IDisposable {
  public readonly List<Entry> Entries=new List<Entry>();public string Name,DisplayName;public uint TitleId,ContentType;
  FileStream stream;long start;List<int> starts=new List<int>();
  public Stfs(string path){Name=System.IO.Path.GetFileName(path);stream=File.OpenRead(path);try{
   var h=Safe.Read(stream,0,0xA000);string magic=Encoding.ASCII.GetString(h,0,4);if((magic!="LIVE"&&magic!="PIRS")||(h[0x37b]&1)==0)throw new InvalidDataException("Unsupported add-on package.");
   TitleId=Safe.BE(h,0x360);ContentType=Safe.BE(h,0x344);if(TitleId!=0x4D5307D3)throw new InvalidDataException("This add-on belongs to a different game.");
   DisplayName=Encoding.BigEndianUnicode.GetString(h,0x411,256).Split('\0')[0];start=((long)Safe.BE(h,0x340)+4095)&~4095L;
   int table=Safe.LE24(h,0x37e),tables=h[0x37c]|h[0x37d]<<8;var paths=new List<string>();var seen=new HashSet<int>();
   if(tables>4096)throw new InvalidDataException("Invalid package table.");
   for(int t=0;t<tables;t++){if(!seen.Add(table))throw new InvalidDataException("Cyclic package table.");int next;var raw=Block(table,out next);table=next;
    for(int o=0;o<4096;o+=64){if(raw[o]==0)break;int len=raw[o+40]&63;if(len>40||len==0)throw new InvalidDataException("Invalid package name.");string leaf=Encoding.UTF8.GetString(raw,o,len);int parent=raw[o+50]<<8|raw[o+51];if(parent!=65535&&parent>=paths.Count)throw new InvalidDataException("Invalid package parent.");string rel=(parent==65535?"":paths[parent]+"/")+leaf;Safe.Child(System.IO.Path.GetTempPath(),rel);paths.Add(rel);
     if((raw[o+40]&128)!=0)continue;Entries.Add(new Entry{Name=rel,Size=Safe.BE(raw,o+52)});starts.Add(Safe.LE24(raw,o+47));
    }
   }
   if(Entries.Select(e=>e.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count()!=Entries.Count)throw new InvalidDataException("Duplicate package files.");
  }catch{Dispose();throw;}}
  byte[] Block(int i,out int next){if(i<0||i>=0xFFFFFF)throw new InvalidDataException("Invalid package block.");long physical=i,level=170;for(int n=0;n<3;n++){physical+=(i+level)/level;if(i<level)break;level*=170;}
   byte[] raw=Safe.Read(stream,start+physical*4096,4096);long hb=i<170?0:(i/170)*171+i/28900+1+(i>=28900?1:0);var hash=Safe.Read(stream,start+hb*4096+(i%170)*24,24);
   using(var sha=SHA1.Create())if(!sha.ComputeHash(raw).SequenceEqual(hash.Take(20)))throw new InvalidDataException("Damaged add-on: block "+i+" failed verification. Select a clean copy.");next=(int)(Safe.BE(hash,20)&0xffffff);return raw;}
  public void Extract(string folder,Action<long> progress,CancellationToken token){for(int n=0;n<Entries.Count;n++){var e=Entries[n];string p=Safe.Child(folder,e.Name);Directory.CreateDirectory(System.IO.Path.GetDirectoryName(p));using(var output=new FileStream(p,FileMode.CreateNew)){long remaining=e.Size;int i=starts[n];var seen=new HashSet<int>();while(remaining>0){token.ThrowIfCancellationRequested();if(!seen.Add(i))throw new InvalidDataException("Cyclic package file.");int next;var b=Block(i,out next);int count=(int)Math.Min(4096,remaining);output.Write(b,0,count);progress(count);remaining-=count;i=next;}}}}
  public byte[] Bytes(Entry e){if(e.Size>16*1024*1024)throw new InvalidDataException("Metadata too large.");using(var output=new MemoryStream()){long remaining=e.Size;int i=starts[Entries.IndexOf(e)];var seen=new HashSet<int>();while(remaining>0){if(!seen.Add(i))throw new InvalidDataException("Cyclic package file.");int next;var b=Block(i,out next);int count=(int)Math.Min(4096,remaining);output.Write(b,0,count);remaining-=count;i=next;}return output.ToArray();}}
  public void Verify(){for(int n=0;n<Entries.Count;n++){long remaining=Entries[n].Size;int i=starts[n];var seen=new HashSet<int>();while(remaining>0){if(!seen.Add(i))throw new InvalidDataException("Cyclic package file.");int next;Block(i,out next);remaining-=Math.Min(4096,remaining);i=next;}}}
  public void Dispose(){if(stream!=null)stream.Dispose();}
 }
 public sealed class Selection {
  public bool UseSharedShaders=true;
  public string Game="",Update="",Bonus="",Target="",TextLanguage="english",Dub="english",UiLanguage="en";
  public List<string> Dlc=new List<string>(),LanguageSources=new List<string>(),TextLanguages=new List<string>(),Dubs=new List<string>();
 }
 public sealed class Status {public string Stage,Detail;public long Done,Total;public double Seconds;}
 public sealed class Plan : IDisposable {
  public bool UsesTu3 {get{return Game.IsRev1||Updates.Count>0;}}
  public Selection Choice;public Disc Game,BonusDisc;public List<Disc> Languages=new List<Disc>();public List<Stfs> Updates=new List<Stfs>(),Dlc=new List<Stfs>();public List<string> TempFolders=new List<string>();public PayloadFile[] Payload;public long Bytes,Free,ModBytes,BonusBytes,InstallBytes;public string Root;
  public void Dispose(){if(Game!=null)Game.Dispose();if(BonusDisc!=null)BonusDisc.Dispose();foreach(var d in Languages)d.Dispose();foreach(var d in Updates.Concat(Dlc))d.Dispose();foreach(var p in TempFolders){try{string parent=System.IO.Path.GetTempPath();if(Path.GetFullPath(p)==Safe.Child(parent,Path.GetFileName(p))&&Path.GetFileName(p).StartsWith("PDZ-source-"))Directory.Delete(p,true);}catch{}}}
 }
 public static class Engine {
  public static readonly string Root=AppDomain.CurrentDomain.BaseDirectory;
  public static readonly string[] DubNames={"english","german","japanese","latam","spanish","french","italian"};
  public static readonly Dictionary<string,int> TextIds=new Dictionary<string,int>{{"english",1},{"japanese",2},{"german",3},{"french",4},{"spanish",5},{"italian",6},{"korean",7},{"chinese",8},{"latam",10}};
  public static PayloadManifest Manifest(){return new JavaScriptSerializer().Deserialize<PayloadManifest>(File.ReadAllText(System.IO.Path.Combine(Root,"payload.json")));}
  public static PayloadFile[] SelectPayload(PayloadManifest manifest,Selection choice,bool tu3) {
   return manifest.files.Where(f=>f.group=="common"||f.group==(tu3?"tu3":"retail")||
    f.group=="bonus"&&choice.Bonus!=""||f.group=="shaders-tu3"&&tu3&&choice.UseSharedShaders).ToArray();
  }
  static IEnumerable<string> PackageFiles(string path,Plan plan) {
   if(Directory.Exists(path))return Safe.Files(path).Where(IsPackage).ToArray();
   if(System.IO.Path.GetExtension(path).Equals(".zip",StringComparison.OrdinalIgnoreCase)){
    string folder=System.IO.Path.Combine(System.IO.Path.GetTempPath(),"PDZ-source-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);plan.TempFolders.Add(folder);var result=new List<string>();
    using(var z=ZipFile.OpenRead(path))foreach(var e in z.Entries){if(e.Length<0xA000||e.Length>1024L*1024*1024)continue;using(var s=e.Open()){var h=new byte[4];if(s.Read(h,0,4)!=4)continue;string magic=Encoding.ASCII.GetString(h);if(magic!="LIVE"&&magic!="PIRS")continue;string dest=Safe.Child(folder,System.IO.Path.GetFileName(e.FullName));using(var o=new FileStream(dest,FileMode.CreateNew)){o.Write(h,0,4);s.CopyTo(o);}result.Add(dest);}}return result;
   }
   return new[]{path};
  }
  static bool IsPackage(string path){try{using(var s=File.OpenRead(path)){var b=new byte[4];return s.Read(b,0,4)==4&&(Encoding.ASCII.GetString(b)=="LIVE"||Encoding.ASCII.GetString(b)=="PIRS");}}catch{return false;}}
  public static bool LanguageEntry(Entry e,string lang,bool dub){if(lang=="chinese")lang="tchinese";string p=e.Name.ToLowerInvariant().Replace('\\','/');return dub?p.StartsWith("pdz/assets/audio/"+lang+"/"):p.StartsWith("pdz/assets/loc/"+lang+"/")||p.StartsWith("fonts"+lang+"/")||p.StartsWith("fonts/"+lang+"/")||p.StartsWith("pdz/assets/fonts/"+lang+"/");}
  public static void ValidateBonus(Disc disc){var e=disc.Entries.SingleOrDefault(x=>x.Name.Equals("default.xex",StringComparison.OrdinalIgnoreCase));if(e==null||Safe.Hash(disc.Bytes(e))!="da7be0f9a446511630c1080078ecc6a68e4dab6e825ee6f6ed98b39e65fb2bde")throw new InvalidDataException("Select the Perfect Dark Zero Limited Edition Bonus DVD, not a game disc.");}
  public static Plan Prepare(Selection choice){return Prepare(choice,CancellationToken.None);}
  public static Plan Prepare(Selection choice,CancellationToken token){var p=new Plan{Choice=choice,Root=Root};try{
   if(String.IsNullOrWhiteSpace(choice.Target))throw new IOException("Choose an installation folder.");choice.Target=System.IO.Path.GetFullPath(choice.Target).TrimEnd('\\');
   if(choice.Target==System.IO.Path.GetPathRoot(choice.Target).TrimEnd('\\')||File.Exists(choice.Target)||Directory.Exists(choice.Target))throw new IOException("Choose a new installation folder. Existing installations and saves will not be replaced.");
   p.Game=new Disc(choice.Game);p.Game.ValidateRetail();
   if(p.Game.IsRev1&&(choice.Update!=""||choice.Dlc.Count>0))throw new InvalidDataException("US Rev 1 already supplies its disc DLC. Remove external DLC and Title Update 3; Reinstated compatibility is prepared automatically.");
   foreach(var s in choice.LanguageSources.Distinct(StringComparer.OrdinalIgnoreCase)){var d=new Disc(s);p.Languages.Add(d);d.ValidateLanguage();}
   var sources=new[]{p.Game}.Concat(p.Languages).ToArray();
   foreach(var lang in choice.TextLanguages)if(!sources.Any(d=>d.Entries.Any(e=>LanguageEntry(e,lang,false))))throw new InvalidDataException("No "+lang+" display-language files found. Add the matching regional disc.");
   foreach(var lang in choice.Dubs)if(!sources.Any(d=>d.Entries.Any(e=>LanguageEntry(e,lang,true))))throw new InvalidDataException("No "+lang+" voice files found. Add the matching regional disc.");
   if(choice.Update!=""&&!p.Game.IsRev1){
    foreach(var file in PackageFiles(choice.Update,p)){var s=new Stfs(file);if(s.Entries.Any(e=>e.Name.Equals("default.xexp",StringComparison.OrdinalIgnoreCase)))p.Updates.Add(s);else s.Dispose();}
    if(p.Updates.Count!=1)throw new InvalidDataException("Select Perfect Dark Zero Title Update 3.");
    var patch=p.Updates[0].Entries.Single(e=>e.Name.Equals("default.xexp",StringComparison.OrdinalIgnoreCase));if(Safe.Hash(p.Updates[0].Bytes(patch))!="2911caf084272fd22199956359e96055539e809e34550600ec73d6d06b0b2c78")throw new InvalidDataException("This title update does not match Title Update 3.");
   }
   foreach(var path in choice.Dlc)foreach(var file in PackageFiles(path,p)){var s=new Stfs(file);if(s.ContentType!=2){s.Dispose();throw new InvalidDataException("The selected file is not a DLC package.");}if(p.Dlc.Any(d=>d.Name==s.Name)){s.Dispose();continue;}p.Dlc.Add(s);}
   if(choice.Dlc.Count>0&&p.Dlc.Count==0)throw new InvalidDataException("No DLC packages found in the selected sources.");
   foreach(var dlc in p.Dlc){try{dlc.Verify();}catch(Exception e){throw new InvalidDataException(dlc.DisplayName+": "+e.Message,e);}}
   if(choice.Bonus!=""){string bonusPath=choice.Bonus;
    if(Path.GetExtension(bonusPath).Equals(".zip",StringComparison.OrdinalIgnoreCase)){using(var zip=ZipFile.OpenRead(bonusPath)){var images=zip.Entries.Where(e=>e.FullName.EndsWith(".iso",StringComparison.OrdinalIgnoreCase)).ToArray();if(images.Length!=1)throw new InvalidDataException("The Bonus DVD ZIP must contain exactly one ISO.");
      if(new DriveInfo(Path.GetPathRoot(Path.GetTempPath())).AvailableFreeSpace<images[0].Length+268435456)throw new IOException("The temporary drive needs "+Safe.Size(images[0].Length+268435456)+" to open the Bonus DVD ZIP. Select an ISO instead, or free space on that drive.");
      string cache=Path.Combine(Path.GetTempPath(),"PDZ-source-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(cache);p.TempFolders.Add(cache);bonusPath=Path.Combine(cache,"Bonus DVD.iso");using(var input=images[0].Open())using(var output=new FileStream(bonusPath,FileMode.CreateNew))Disc.CopyStream(input,output,images[0].Length,n=>{},token);
     }}p.BonusDisc=new Disc(bonusPath);ValidateBonus(p.BonusDisc);p.BonusBytes=p.BonusDisc.Entries.Sum(e=>e.Size);
   }
   p.Payload=SelectPayload(Manifest(),choice,p.UsesTu3);p.ModBytes=p.Payload.Where(f=>f.group!="bonus").Sum(f=>f.size);
   var installed=new Dictionary<string,long>(StringComparer.OrdinalIgnoreCase);
   foreach(var e in p.Game.Entries.Where(e=>!e.Name.StartsWith("$SystemUpdate",StringComparison.OrdinalIgnoreCase)))installed["assets/"+e.Name]=e.Size;
   p.Bytes=installed.Values.Sum()+p.Payload.Sum(f=>f.size)+p.Updates.Concat(p.Dlc).Sum(s=>s.Entries.Sum(e=>e.Size));
   foreach(var d in p.Languages)foreach(var e in d.Entries.Where(e=>choice.TextLanguages.Any(l=>LanguageEntry(e,l,false))||choice.Dubs.Any(l=>LanguageEntry(e,l,true)))){string name="assets/"+e.Name;if(installed.ContainsKey(name))continue;installed[name]=e.Size;p.Bytes+=e.Size;}
   foreach(var s in p.Updates)foreach(var e in s.Entries){installed["update/"+e.Name]=e.Size;if(e.Name.Equals("default.xexp",StringComparison.OrdinalIgnoreCase)){installed["assets/default.xexp"]=e.Size;p.Bytes+=e.Size;}}
   foreach(var s in p.Dlc)foreach(var e in s.Entries)installed["user/0000000000000000/4D5307D3/00000002/"+s.Name+"/"+e.Name]=e.Size;
   foreach(var f in p.Payload)installed[f.destination.Replace('\\','/')]=f.size;
   if(p.Game.IsRev1){installed["assets/default.xex"]=Rev1.OutputSize;p.Bytes+=Rev1.OutputSize+0x13e0000;Rev1.ValidateTools();}
   p.InstallBytes=installed.Values.Sum()+p.BonusBytes;
   p.Bytes+=p.BonusBytes;p.Free=new DriveInfo(System.IO.Path.GetPathRoot(choice.Target)).AvailableFreeSpace;
   if(p.Free<p.Bytes+256L*1024*1024)throw new IOException("Not enough free space. Needed: "+Safe.Size(p.Bytes+256L*1024*1024)+". Available: "+Safe.Size(p.Free)+".");
   foreach(var f in p.Payload){string file=Safe.Child(Root,f.source);if(!File.Exists(file)||new FileInfo(file).Length!=f.size)throw new IOException("Installer payload missing or incomplete: "+f.source);Safe.Child(choice.Target,f.destination);}
   return p;
  }catch{p.Dispose();throw;}}
  public static void Install(Plan plan,IProgress<Status> report,CancellationToken token) {
   string parent=System.IO.Path.GetDirectoryName(plan.Choice.Target);Directory.CreateDirectory(parent);string stage=System.IO.Path.Combine(parent,".pdz-install-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stage);
   var watch=Stopwatch.StartNew();long done=0;string phase="Preparing installation",detail="";DateTime last=DateTime.MinValue;
   Action<long> progress=n=>{done+=n;if((DateTime.UtcNow-last).TotalMilliseconds<120)return;last=DateTime.UtcNow;report.Report(new Status{Stage=phase,Detail=detail,Done=done,Total=plan.Bytes,Seconds=watch.Elapsed.TotalSeconds});};
   Action<string,string> copy=(src,dest)=>{Directory.CreateDirectory(System.IO.Path.GetDirectoryName(dest));using(var input=File.OpenRead(src))using(var output=new FileStream(dest,FileMode.Create))Disc.CopyStream(input,output,input.Length,progress,token);};
   try {
    token.ThrowIfCancellationRequested();string assets=System.IO.Path.Combine(stage,"assets");Directory.CreateDirectory(assets);phase="Extracting game files";
    if(!plan.Game.IsFolder){
     string extractor=System.IO.Path.Combine(Root,"tools/extract-xiso.exe");if(!File.Exists(extractor))throw new FileNotFoundException("The ISO extractor is missing.",extractor);
     var psi=new ProcessStartInfo(extractor,"-x -s -d "+Safe.Quote(assets)+" "+Safe.Quote(plan.Game.PathName)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
     var errors=new StringBuilder();using(var process=Process.Start(psi)){process.OutputDataReceived+=(s,e)=>{};process.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)lock(errors){if(errors.Length<8192)errors.AppendLine(e.Data);}};process.BeginOutputReadLine();process.BeginErrorReadLine();long measured=0;
      while(!process.WaitForExit(500)){if(token.IsCancellationRequested){process.Kill();process.WaitForExit();token.ThrowIfCancellationRequested();}long bytes=Safe.Files(assets).Sum(f=>new FileInfo(f).Length);progress(Math.Max(0,bytes-measured));measured=bytes;}process.WaitForExit();long final=Safe.Files(assets).Sum(f=>new FileInfo(f).Length);progress(Math.Max(0,final-measured));if(process.ExitCode!=0)throw new IOException("ISO extraction failed: "+errors);
     }
     foreach(var e in plan.Game.Entries.Where(e=>!e.Name.StartsWith("$SystemUpdate",StringComparison.OrdinalIgnoreCase))){string file=Safe.Child(assets,e.Name);if(!File.Exists(file)||new FileInfo(file).Length!=e.Size)throw new IOException("ISO extraction incomplete: "+e.Name);}
    }else foreach(var e in plan.Game.Entries.Where(e=>!e.Name.StartsWith("$SystemUpdate",StringComparison.OrdinalIgnoreCase))){detail=e.Name;plan.Game.Copy(e,Safe.Child(assets,e.Name),progress,token);}
    if(Safe.Hash(System.IO.Path.Combine(assets,"default.xex"))!=plan.Game.ExecutableHash)throw new InvalidDataException("The extracted executable failed verification.");
    if(plan.Game.IsRev1){phase="Preparing Rev 1 compatibility";progress(0);Rev1.Normalize(assets,stage,token);}
    phase="Installing title update";foreach(var s in plan.Updates){string update=System.IO.Path.Combine(stage,"update");s.Extract(update,progress,token);string patch=System.IO.Path.Combine(update,"default.xexp");if(Safe.Hash(patch)!="2911caf084272fd22199956359e96055539e809e34550600ec73d6d06b0b2c78")throw new InvalidDataException("This title update does not match the supported Title Update 3.");
     // ReXGlue discovers the executable patch beside default.xex. update: is
     // still mounted for the game's updated data files.
     copy(patch,System.IO.Path.Combine(assets,"default.xexp"));}
    phase="Installing map packs";foreach(var s in plan.Dlc){detail=s.DisplayName;s.Extract(Safe.Child(stage,"user/0000000000000000/4D5307D3/00000002/"+s.Name),progress,token);}
    phase="Installing display and voice languages";
    foreach(var d in plan.Languages)foreach(var e in d.Entries.Where(e=>plan.Choice.TextLanguages.Any(l=>LanguageEntry(e,l,false))||plan.Choice.Dubs.Any(l=>LanguageEntry(e,l,true)))){
     detail=e.Name;string dest=Safe.Child(assets,e.Name);if(File.Exists(dest))continue;d.Copy(e,dest,progress,token);
    }
    phase="Installing and verifying the recomp mod";foreach(var f in plan.Payload){token.ThrowIfCancellationRequested();detail=f.destination;string dest=Safe.Child(stage,f.destination);copy(Safe.Child(Root,f.source),dest);if(Safe.Hash(dest)!=f.sha256)throw new InvalidDataException("Payload checksum failed: "+f.destination);}
    if(plan.BonusDisc!=null){phase="Installing Bonus DVD content";foreach(var e in plan.BonusDisc.Entries){detail=e.Name;plan.BonusDisc.Copy(e,Safe.Child(stage,"bonus/contents/"+e.Name),progress,token);}}
    phase="Finishing installation";detail="";token.ThrowIfCancellationRequested();
    string exe=plan.UsesTu3?"PDZReinstatedTU3.exe":"perfectdarkzerorecomp.exe";
    string hardware="gpu_plugin = \"xenos\"\npdz_unlock_all_missions = false\npdz_walk_speed = 20.0\npdz_cutscene_timing_fix = true\nmnk_mode = true\nmnk_mouse = true\npdz_mouse_menu_navigation = true\nasync_shader_compilation = true\nuser_language = "+TextIds[plan.Choice.TextLanguage]+"\npdz_dub_language = "+Array.IndexOf(DubNames,plan.Choice.Dub)+"\ndev_debug_runtime = false\n";
    Safe.Write(System.IO.Path.Combine(stage,"settings/hardware.toml"),hardware);
    Safe.Write(System.IO.Path.Combine(stage,"installation.json"),new JavaScriptSerializer().Serialize(new{version=Manifest().version,titleUpdate=plan.UsesTu3?3:0,sourceRevision=plan.Game.IsRev1?1:0,sharedShaders=plan.Payload.Any(f=>f.group=="shaders-tu3"),displayLanguage=plan.Choice.TextLanguage,dub=plan.Choice.Dub,installedAt=DateTime.UtcNow.ToString("o")}));
    if(Directory.Exists(plan.Choice.Target)||File.Exists(plan.Choice.Target))throw new IOException("The destination was created by another application. No existing files were replaced.");
    string vlcSupport=Path.Combine(stage,"bonus/vlc");if(Directory.Exists(vlcSupport))File.SetAttributes(vlcSupport,File.GetAttributes(vlcSupport)|FileAttributes.Hidden);
    Directory.Move(stage,plan.Choice.Target);report.Report(new Status{Stage="Installation complete!",Detail=plan.Choice.Target,Done=plan.Bytes,Total=plan.Bytes,Seconds=watch.Elapsed.TotalSeconds});
   }catch(Exception error){try{Safe.Write(stage+"-error.txt",error.ToString());string verified=Safe.Child(parent,Path.GetFileName(stage));if(Path.GetFullPath(stage)==verified&&Path.GetFileName(stage).StartsWith(".pdz-install-")&&Directory.Exists(stage))Directory.Delete(stage,true);}catch{}throw;}
  }
 }
}
