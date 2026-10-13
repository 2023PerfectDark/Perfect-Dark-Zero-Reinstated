param(
  [ValidateSet('Check','Download','Redownload','Install')][string]$Action,
  [Parameter(Mandatory=$true)][string]$Root,
  [int]$GameProcessId = 0
)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Root = [IO.Path]::GetFullPath($Root).TrimEnd('\')
$work = Join-Path $Root '.pdz-update'
$sourcePath = Join-Path $Root 'mod_assets\updater\source.json'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$lock = $null
function State([string]$phase, [int]$percent, [string]$message) {
  $text = $phase + "`n" + $percent + "`n" + ($message -replace '[\r\n]', ' ') + "`n"
  $tmp = Join-Path $work 'status.tmp'
  [IO.File]::WriteAllText($tmp,$text,[Text.Encoding]::UTF8)
  Move-Item -LiteralPath $tmp -Destination (Join-Path $work 'status.txt') -Force
}
function JsonWrite($value,[string]$path) {
  [IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 12),[Text.Encoding]::UTF8)
}
function FileHash([string]$path) {
  $stream = [IO.File]::OpenRead($path); $sha = [Security.Cryptography.SHA256]::Create()
  try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
  finally { $stream.Dispose(); $sha.Dispose() }
}
function Allowed([string]$name) {
  # Updates never overwrite user configuration, profiles, saves or base-game assets.
  return $name -in @('perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll','gamecontrollerdb.txt',
    'out/surface/terrain.pdzsurf','out/surface/mission-music.wav') -or
    ($name.StartsWith('mod_assets/',[StringComparison]::OrdinalIgnoreCase) -and
     $name -ine 'mod_assets/updater/source.json')
}
function SafeRelative([string]$name) {
  if ($name -match '[\\:]' -or $name.StartsWith('/') -or $name -match '(^|/)\.\.?(/|$)' -or
      $name -match '[\x00-\x1f]' -or $name -match '(^|/)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|/|$)' -or
      $name -match '[ .](/|$)') { throw 'Unsafe path in update archive.' }
  if (-not $name.StartsWith('PDZ-Recomp/',[StringComparison]::Ordinal)) { throw 'Unexpected archive root.' }
  return $name.Substring(11)
}
function NoLinks([string]$path) {
  $p = $path
  while ($p.Length -ge $Root.Length) {
    if (Test-Path -LiteralPath $p) {
      if ((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw 'Update destination contains a filesystem link.'
      }
    }
    if ($p -ieq $Root) { break }
    $p = [IO.Path]::GetDirectoryName($p)
  }
}
try {
  NoLinks $work
  $lock = [IO.File]::Open((Join-Path $work 'lock'), 'OpenOrCreate','ReadWrite','None')
  if ($Action -eq 'Install') {
    if ($GameProcessId -le 0) { throw 'Missing game process.' }
    $game = Get-Process -Id $GameProcessId -ErrorAction SilentlyContinue
    if ($game) {
      if ($game.Path -ine (Join-Path $Root 'perfectdarkzerorecomp.exe')) { throw 'Wrong game process.' }
      if (-not $game.WaitForExit(120000)) { throw 'Game did not close. Update was not installed.' }
    }
    $plan = Get-Content -LiteralPath (Join-Path $work 'plan.json') -Raw | ConvertFrom-Json
    $backup = Join-Path $work ('backup-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    $changed = [Collections.Generic.List[object]]::new()
    try {
      foreach ($entry in $plan.files) {
        $rel = SafeRelative ('PDZ-Recomp/' + $entry.name)
        if (-not (Allowed $rel)) { throw 'Update plan contains a protected file.' }
        $src = Join-Path (Join-Path $work 'staged') $rel
        $dst = Join-Path $Root $rel
        NoLinks $src; NoLinks $dst
        if ((FileHash $src) -ine $entry.sha256) { throw 'Staged update changed.' }
        $old = Join-Path $backup $rel
        $existed = Test-Path -LiteralPath $dst
        if ($existed) {
          New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($old)) -Force | Out-Null
          Copy-Item -LiteralPath $dst -Destination $old
        }
        $changed.Add(@{path=$dst;old=$old;existed=$existed})
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dst)) -Force | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
        if ((FileHash $dst) -ine $entry.sha256) { throw 'Installed file verification failed.' }
      }
      $config = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json
      $config.installedTag = $plan.tag
      $config.installedPublishedAt = $plan.published
      JsonWrite $config $sourcePath
      JsonWrite @{ tag=$plan.tag; executableHash=(FileHash (Join-Path $Root 'perfectdarkzerorecomp.exe')) } (Join-Path $work 'success.json')
      State 'installed' 100 'Update installed.'
    } catch {
      foreach ($entry in $changed) {
        if ($entry.existed) { Copy-Item -LiteralPath $entry.old -Destination $entry.path -Force }
        elseif (Test-Path -LiteralPath $entry.path) { Remove-Item -LiteralPath $entry.path }
      }
      throw
    }
    Start-Process -FilePath (Join-Path $Root 'perfectdarkzerorecomp.exe') -WorkingDirectory $Root
    exit 0
  }
  $config = Get-Content -LiteralPath $sourcePath -Raw | ConvertFrom-Json
  if (-not $config.repository) { State 'unconfigured' 0 'The update source is not configured yet.'; exit 0 }
  if ($config.repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $config.asset -notmatch '^[A-Za-z0-9_.-]+\.zip$') { throw 'Invalid update source configuration.' }
  $headers = @{ 'User-Agent'='PDZ-Modded-Recomp-Updater'; Accept='application/vnd.github+json'; 'X-GitHub-Api-Version'='2022-11-28' }
  State 'checking' 0 'Checking for updates...'
  $release = Invoke-RestMethod -Uri ('https://api.github.com/repos/' + $config.repository + '/releases/latest') -Headers $headers -TimeoutSec 30
  if ($release.draft -or $release.prerelease) { throw 'Release is not a stable published update.' }
  if ($Action -ne 'Redownload' -and ($release.tag_name -eq $config.installedTag -or [DateTimeOffset]::Parse($release.published_at) -le [DateTimeOffset]::Parse($config.installedPublishedAt))) {
    State 'current' 100 'Your modded recomp is up to date.'; exit 0
  }
  $assets = @($release.assets | Where-Object { $_.name -ceq $config.asset -and $_.state -eq 'uploaded' })
  if ($assets.Count -ne 1) { throw 'The release does not have the configured update package.' }
  $asset = $assets[0]
  if ($asset.digest -notmatch '^sha256:([0-9a-fA-F]{64})$') { throw 'GitHub has not provided a SHA256 digest for this package.' }
  $expected = $Matches[1]
  $url = [Uri]$asset.browser_download_url
  $downloadRepository = $config.repository
  # Preserve existing source configuration across the project's GitHub rename.
  if ($downloadRepository -ieq '2023PerfectDark/PerfectDarkZeroRecomp') {
    $downloadRepository = '2023PerfectDark/Perfect-Dark-Zero-Reinstated'
  }
  $expectedPath = $url.AbsolutePath.StartsWith('/' + $config.repository + '/releases/download/',[StringComparison]::OrdinalIgnoreCase) -or
    $url.AbsolutePath.StartsWith('/' + $downloadRepository + '/releases/download/',[StringComparison]::OrdinalIgnoreCase)
  if ($url.Scheme -ne 'https' -or $url.Host -ne 'github.com' -or -not $expectedPath) { throw 'Unexpected download location.' }
  if ($Action -eq 'Check') { State 'available' 0 'An update is available. Select Download update.'; exit 0 }
  if ($asset.size -le 0 -or $asset.size -gt 2147483648) { throw 'Unexpected package size.' }
  $zipPath = Join-Path $work 'download.zip'
  State 'downloading' 0 'Downloading update...'
  $request = [Net.HttpWebRequest]::Create($url)
  $request.UserAgent = 'PDZ-Modded-Recomp-Updater'
  $request.Timeout = 30000; $request.ReadWriteTimeout = 30000
  $response = $request.GetResponse()
  try {
    if ($response.ResponseUri.Scheme -ne 'https') { throw 'Insecure download redirect.' }
    $inputStream = $response.GetResponseStream()
    $output = [IO.File]::Create($zipPath)
    try {
      $buffer = New-Object byte[] 131072; $received = [long]0; $last = -1
      while (($count=$inputStream.Read($buffer,0,$buffer.Length)) -gt 0) {
        $received += $count
        if ($received -gt $asset.size) { throw 'Package is larger than advertised.' }
        $output.Write($buffer,0,$count)
        $percent = [int][Math]::Floor(100.0*$received/$asset.size)
        if ($percent -ne $last) { State 'downloading' $percent 'Downloading update...'; $last=$percent }
      }
      if ($received -ne $asset.size) { throw 'Download is incomplete.' }
    } finally { $output.Dispose(); $inputStream.Dispose() }
  } finally { $response.Dispose() }
  State 'verifying' 100 'Verifying update...'
  if ((FileHash $zipPath) -ine $expected) { throw 'Download checksum does not match GitHub.' }
  $archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
  try {
    $manifestEntry = $archive.GetEntry('PDZ-Recomp/SHA256SUMS.json')
    if (-not $manifestEntry -or $manifestEntry.Length -gt 1048576) { throw 'Update manifest is missing or too large.' }
    $reader = [IO.StreamReader]::new($manifestEntry.Open())
    try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $files = @(); $total = [long]0
    foreach ($entry in $archive.Entries) {
      $rel = SafeRelative $entry.FullName
      if (-not $names.Add($rel)) { throw 'Duplicate archive path.' }
      if (($entry.ExternalAttributes -shr 16 -band 0xF000) -eq 0xA000) { throw 'Links are not allowed in updates.' }
      $total += $entry.Length
      if ($total -gt 4294967296 -or $names.Count -gt 10000) { throw 'Expanded package exceeds update limits.' }
      if (-not (Allowed $rel) -or $entry.FullName.EndsWith('/')) { continue }
      $hash = $manifest.PSObject.Properties[$rel].Value
      if ($hash -notmatch '^[a-fA-F0-9]{64}$') { throw 'File checksum missing from manifest.' }
      $dst = Join-Path (Join-Path $work 'staged') $rel
      NoLinks $dst
      New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dst)) -Force | Out-Null
      [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$dst,$true)
      if ((FileHash $dst) -ine $hash) { throw 'Extracted file checksum mismatch.' }
      $files += @{name=$rel;sha256=$hash}
    }
    foreach ($required in @('perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll')) {
      if ($required -notin $files.name) { throw 'Update is missing a required game binary.' }
    }
    JsonWrite @{tag=$release.tag_name;published=$release.published_at;files=$files} (Join-Path $work 'plan.json')
  } finally { $archive.Dispose() }
  State 'ready' 100 'Update ready. Restart now or wait.'
} catch {
  State 'error' 0 ('Update failed: ' + $_.Exception.Message)
  exit 1
} finally { if ($lock) { $lock.Dispose() } }
