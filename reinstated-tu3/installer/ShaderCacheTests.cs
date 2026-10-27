using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Web.Script.Serialization;

namespace PdzInstaller {
 static class ShaderCacheTests {
  static void Require(bool condition,string message){if(!condition)throw new Exception(message);}
  public static int Main(string[] args){try{
   var manifest=Engine.Manifest();var choice=new Selection();
   var shared=Engine.SelectPayload(manifest,choice,true);
   var cache=shared.Where(f=>f.group=="shaders-tu3").ToArray();
   Require(cache.Length==4,"Expected four portable cache files");
   Require(cache.All(f=>f.destination.StartsWith("user/cache/shaders/shareable/")),"Unexpected cache destination");
   Require(cache.All(f=>f.destination.EndsWith(".xsh")||f.destination.EndsWith(".xpso")),"Non-portable cache file");
   choice.UseSharedShaders=false;var own=Engine.SelectPayload(manifest,choice,true);
   Require(!own.Any(f=>f.destination.StartsWith("user/cache/")),"Own-cache installation must start empty");
   Require(shared.Sum(f=>f.size)-own.Sum(f=>f.size)==cache.Sum(f=>f.size),"Space accounting mismatch");
   Require(shared.Except(cache).SequenceEqual(own),"Shader choice changed another payload");
   choice.UseSharedShaders=true;
   Require(!Engine.SelectPayload(manifest,choice,false).Any(f=>f.group=="shaders-tu3"),"TU3 cache leaked into retail build");
   var json=new JavaScriptSerializer();choice.UseSharedShaders=false;
   Require(!json.Deserialize<Selection>(json.Serialize(choice)).UseSharedShaders,"Choice did not persist");
   string root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);
   string fixture=Path.Combine(root,"game-source");Directory.CreateDirectory(fixture);
   File.Copy(args[1],Path.Combine(fixture,"default.xex"),false);
   foreach(bool useCache in new[]{true,false}){
    choice=new Selection{UseSharedShaders=useCache,Target=Path.Combine(root,useCache?"shared":"own")};
    // Exercise the actual install/copy/verification path with a minimal valid
    // executable fixture, rather than copying a complete game twice.
    var selected=Engine.SelectPayload(manifest,choice,true).Where(f=>f.group=="shaders-tu3").ToArray();
    using(var plan=new Plan{Choice=choice,Game=new Disc(fixture),Payload=selected,Bytes=new FileInfo(args[1]).Length+selected.Sum(f=>f.size)}){
     Engine.Install(plan,new Progress<Status>(),CancellationToken.None);
    }
    Require(Directory.Exists(Path.Combine(choice.Target,"user/cache"))==useCache,"Wrong installed cache state");
    foreach(var file in selected)Require(Safe.Hash(Safe.Child(choice.Target,file.destination))==file.sha256,"Installed cache checksum mismatch");
    var meta=json.Deserialize<System.Collections.Generic.Dictionary<string,object>>(File.ReadAllText(Path.Combine(choice.Target,"installation.json")));
    Require((bool)meta["sharedShaders"]==useCache,"Installation receipt differs from selected cache");
   }
   Console.WriteLine("PASS: cache selection, retail exclusion, size accounting, serialization, both install paths and installed hashes");return 0;
  }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
 }
}
