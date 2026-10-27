using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;

// The distributable embeds the verified release ZIP. Its private extraction
// directory lives only for the lifetime of the actual installer process.
static class Bootstrap {
  [STAThread] static int Main(string[] args) {
    bool verify = args.Length == 1 && args[0] == "--verify-package";
    string root = Path.Combine(Path.GetTempPath(), "PDZ-Setup-" + Guid.NewGuid().ToString("N"));
    int result = 1;
    Application.EnableVisualStyles();
    using (var window = new Form()) {
      window.Text = "Perfect Dark Zero: Reinstated";
      window.Width = 460; window.Height = 135;
      window.StartPosition = FormStartPosition.CenterScreen;
      window.FormBorderStyle = FormBorderStyle.FixedDialog;
      window.MaximizeBox = false; window.MinimizeBox = false; window.ControlBox = false;
      var label = new Label { Text = "Preparing installer...", Left = 22, Top = 18, Width = 400 };
      var progress = new ProgressBar { Left = 22, Top = 48, Width = 400, Style = ProgressBarStyle.Marquee };
      window.Controls.Add(label); window.Controls.Add(progress);
      window.Shown += delegate {
        new Thread(delegate() {
          try {
            Directory.CreateDirectory(root);
            string prefix = Path.GetFullPath(root) + Path.DirectorySeparatorChar;
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip"))
            using (var zip = new ZipArchive(stream, ZipArchiveMode.Read)) {
              foreach (var entry in zip.Entries) {
                // Release ZIPs have a long descriptive top-level directory.
                // Omit it inside our already unique temporary directory.
                int slash = entry.FullName.IndexOf('/');
                if (slash < 0) throw new InvalidDataException("Package root missing.");
                string relative = entry.FullName.Substring(slash + 1);
                if (relative.Length == 0) continue;
                string target = Path.GetFullPath(Path.Combine(root, relative));
                if (!target.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Invalid package path.");
                if (entry.FullName.EndsWith("/")) { Directory.CreateDirectory(target); continue; }
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                using (var input = entry.Open())
                using (var output = new FileStream(target, FileMode.CreateNew)) input.CopyTo(output);
              }
            }
            string[] setups = Directory.GetFiles(root, "PDZ Setup.exe", SearchOption.AllDirectories);
            if (setups.Length != 1) throw new InvalidDataException("Installer executable missing or ambiguous.");
            string folder = Path.GetDirectoryName(setups[0]);
            if (!File.Exists(Path.Combine(folder, "payload.json"))) throw new InvalidDataException("Installer manifest missing.");
            if (!verify) {
              window.Invoke((Action)delegate { window.Hide(); });
              using (var child = Process.Start(new ProcessStartInfo(setups[0]) { WorkingDirectory = folder, UseShellExecute = false })) {
                child.WaitForExit(); result = child.ExitCode;
              }
            } else result = 0;
          } catch (Exception ex) {
            if (verify) File.WriteAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "package-verification-error.txt"), ex.ToString());
            if (!verify) MessageBox.Show(ex.Message, "Unable to prepare PDZ installer", MessageBoxButtons.OK, MessageBoxIcon.Error);
            result = 1;
          } finally {
            // Only delete the unique directory created by this invocation.
            try { if (Directory.Exists(root)) Directory.Delete(root, true); } catch { }
            window.BeginInvoke((Action)delegate { window.Close(); });
          }
        }).Start();
      };
      Application.Run(window);
    }
    return result;
  }
}
