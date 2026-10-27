using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Web.Script.Serialization;
namespace PdzInstaller {
 static class Verify {
  static int failures;
  static void Test(string name,Action a){try{a();Console.WriteLine("PASS "+name);}catch(Exception e){failures++;Console.WriteLine("FAIL "+name+": "+e.Message);}}
  static void Rejected(Action a){try{a();}catch(InvalidDataException){return;}catch(IOException){return;}throw new Exception("Input was unexpectedly accepted");}
  public static int Main(string[] args){try{
   if(args.Length>0&&args[0]=="--retail-index"){using(var d=new Disc(args[1]))File.WriteAllText(Path.Combine(Engine.Root,"retail-index.json"),new JavaScriptSerializer().Serialize(d.Entries.Where(e=>!e.Name.StartsWith("$SystemUpdate",StringComparison.OrdinalIgnoreCase)).Select(e=>new{Name=e.Name,Size=e.Size}).ToArray()));return 0;}
   if(args.Length>0&&(args[0]=="--plan"||args[0]=="--cancel")){var s=new JavaScriptSerializer().Deserialize<Selection>(File.ReadAllText(args[1]));using(var p=Engine.Prepare(s)){Console.WriteLine("PLAN mod="+p.ModBytes+" installed="+p.InstallBytes+" transfer="+p.Bytes+" titleUpdate="+p.Updates.Count+" bonus="+p.BonusBytes);if(args[0]=="--cancel"){var token=new CancellationToken(true);try{Engine.Install(p,new Progress<Status>(),token);throw new Exception("Cancellation ignored");}catch(OperationCanceledException){if(Directory.Exists(s.Target))throw new Exception("Cancelled target was published");Console.WriteLine("CANCEL ROLLBACK OK");}}}return 0;}
   if(args.Length>0&&args[0]=="--install"){var s=new JavaScriptSerializer().Deserialize<Selection>(File.ReadAllText(args[1]));using(var p=Engine.Prepare(s)){Console.WriteLine("Prepared: mod="+p.ModBytes+" work="+p.Bytes+" free="+p.Free);var report=new Progress<Status>(x=>Console.WriteLine(x.Stage+" | "+x.Done+"/"+x.Total));Engine.Install(p,report,CancellationToken.None);}Console.WriteLine("INSTALL OK");return 0;}
   string h=@"H:\Perfect Dark Zero Xbox360";
   Test("unsafe paths rejected",()=>{foreach(var n in new[]{"../x","a/../b","C:/x","/x","a:ads","CON","Lpt1.txt","a./b","a//b"})Rejected(()=>Safe.Child(Engine.Root,n));});
   Test("Chinese folder mapping",()=>{if(!Engine.LanguageEntry(new Entry{Name="pdz/ASSETS/loc/tchinese/PackageData/test.pkg"},"chinese",false)||!Engine.LanguageEntry(new Entry{Name="fontstchinese/FontTable.txt"},"chinese",false))throw new Exception("Chinese files missing");});
   Test("original retail ISO accepted",()=>{using(var d=new Disc(Path.Combine(h,@"Perfect Dark Zero (USA) (En,Fr,Es)\Perfect Dark Zero (USA) (En,Fr,Es).iso"))){d.ValidateRetail();Console.WriteLine("Disc files "+d.Entries.Count+" / "+d.Entries.Sum(e=>e.Size));}});
   Test("Rev 1 rejected",()=>Rejected(()=>{using(var d=new Disc(Path.Combine(h,@"Perfect Dark Zero (USA) (En,Fr,Es) (Rev 1)\Perfect Dark Zero (USA) (En,Fr,Es) (Rev 1).iso")))d.ValidateRetail();}));
   Test("regional discs accepted",()=>{foreach(var folder in new[]{"Perfect Dark Zero (Europe) (En,Fr,De)","Perfect Dark Zero (Europe) (En,Es,It)","Perfect Dark Zero (Japan, Korea) (En,Ja,Zh,Ko)"})using(var d=new Disc(Directory.GetFiles(Path.Combine(h,folder),"*.iso").Single()))d.ValidateLanguage();});
   Test("payload checksums",()=>{foreach(var f in Engine.Manifest().files)if(Safe.Hash(Safe.Child(Engine.Root,f.source))!=f.sha256)throw new Exception(f.source);});
   Console.WriteLine(failures==0?"ALL CHECKS PASSED":failures+" FAILED");return failures==0?0:1;
  }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
 }
}
