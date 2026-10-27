using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Web.Script.Serialization;

namespace PdzInstaller {
 public static class Rev1 {
  public const string ImageHash="a3ef12d51bfba1c35127d5f9bb349beaf345de64a7133c25d0ff1023d0c1cb8e";
  public const string OutputHash="f6fbe8935aaf164888e3e662344d8426316066684fd5ecec4b6a732599734916";
  public const int OutputSize=20856832;
  public static void ValidateTools(){
   var files=new JavaScriptSerializer().Deserialize<PayloadFile[]>(File.ReadAllText(Path.Combine(Engine.Root,"rev1-tools.json")));
   var expected=new System.Collections.Generic.HashSet<string>(new[]{"tools/rev1-image.exe","tools/rexruntime.dll","tools/rev1-tu3.delta"},StringComparer.OrdinalIgnoreCase);
   if(files==null||files.Length!=3)throw new InvalidDataException("Incomplete Rev 1 tools manifest.");
   foreach(var f in files)if(f==null||!expected.Remove(f.source))throw new InvalidDataException("Unexpected Rev 1 compatibility tool.");
   foreach(var f in files){string path=Safe.Child(Engine.Root,f.source);if(!File.Exists(path)||new FileInfo(path).Length!=f.size||Safe.Hash(path)!=f.sha256)throw new InvalidDataException("Rev 1 compatibility tool failed verification: "+f.source);}
  }
  [StructLayout(LayoutKind.Sequential)] struct Input {public IntPtr Data;public UIntPtr Size;public int Editable;}
  [DllImport("msdelta.dll",SetLastError=true)] static extern bool ApplyDeltaProvidedB(long flags,Input source,Input delta,IntPtr target,UIntPtr size);
  public static byte[] Apply(byte[] source,byte[] delta) {
   if(source.Length!=0x13e0000||Safe.Hash(source)!=ImageHash)throw new InvalidDataException("Rev 1 executable image failed verification.");
   if(delta.Length==0||delta.Length>1024*1024)throw new InvalidDataException("Invalid Rev 1 compatibility patch.");
   var result=new byte[OutputSize];var a=GCHandle.Alloc(source,GCHandleType.Pinned);var b=GCHandle.Alloc(delta,GCHandleType.Pinned);var c=GCHandle.Alloc(result,GCHandleType.Pinned);
   try {if(!ApplyDeltaProvidedB(0,new Input{Data=a.AddrOfPinnedObject(),Size=(UIntPtr)source.Length},new Input{Data=b.AddrOfPinnedObject(),Size=(UIntPtr)delta.Length},c.AddrOfPinnedObject(),(UIntPtr)result.Length))throw new InvalidDataException("Unable to apply Rev 1 compatibility patch.");}
   finally{c.Free();b.Free();a.Free();}
   if(Safe.Hash(result)!=OutputHash)throw new InvalidDataException("Rev 1 compatibility output failed verification.");return result;
  }
  public static void Normalize(string assets,string stage,CancellationToken token) {
   ValidateTools();
   if(Safe.Hash(Path.Combine(assets,"default.xex"))!=Disc.Rev1Hash)throw new InvalidDataException("Rev 1 source failed verification.");
   string work=Safe.Child(stage,"rev1-conversion");Directory.CreateDirectory(work);
   // Run beside the verified runtime, never search the user's PATH for a DLL.
   foreach(string name in new[]{"rev1-image.exe","rexruntime.dll"})File.Copy(Path.Combine(Engine.Root,"tools",name),Path.Combine(work,name));
   string output=Path.Combine(work,"image.bin");
   var start=new ProcessStartInfo(Path.Combine(work,"rev1-image.exe"),Safe.Quote(assets)+" "+Safe.Quote(output)){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=work,RedirectStandardOutput=true,RedirectStandardError=true};
   using(var process=Process.Start(start)){process.OutputDataReceived+=(s,e)=>{};process.ErrorDataReceived+=(s,e)=>{};process.BeginOutputReadLine();process.BeginErrorReadLine();var watch=Stopwatch.StartNew();
    while(!process.WaitForExit(200)){if(token.IsCancellationRequested||watch.Elapsed.TotalSeconds>120){process.Kill();process.WaitForExit();token.ThrowIfCancellationRequested();throw new IOException("Rev 1 conversion timed out.");}}
    process.WaitForExit();if(process.ExitCode!=0)throw new IOException("Rev 1 image reader failed ("+process.ExitCode+").");}
   token.ThrowIfCancellationRequested();
   if(!File.Exists(output)||new FileInfo(output).Length!=0x13e0000)throw new InvalidDataException("Incomplete Rev 1 executable image.");
   var converted=Apply(File.ReadAllBytes(output),File.ReadAllBytes(Path.Combine(Engine.Root,"tools/rev1-tu3.delta")));
   File.WriteAllBytes(Path.Combine(assets,"default.xex"),converted);
   // This private staging directory was created above and never uses source paths.
   if(Path.GetFullPath(work)!=Safe.Child(stage,"rev1-conversion"))throw new IOException("Invalid conversion staging path.");
   Directory.Delete(work,true);
  }
 }
}
