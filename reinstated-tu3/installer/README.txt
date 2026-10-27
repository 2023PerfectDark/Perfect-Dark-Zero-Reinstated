PERFECT DARK ZERO: REINSTATED
Release v124 - 27 October 2026
TU3 base 1.8; game build v124

PLAYER-LIST HIGHLIGHTS v123
Fixes stale gamertag highlights in the multiplayer lobby and Connected
Players screen. The user confirmed both screens highlight one name at a
time in the installed build. Targeted host tests cover visual reset,
selection changes, scrolling/recycled rows, and disabled rows; these
tests do not replace broader in-game keyboard/click testing.
Random bot head/body buttons remain a separate unresolved issue.

UPDATER AND PACKAGING v121
The installer now includes its update source configuration and targets
2023PerfectDark/Perfect-Dark-Zero-Reinstated. Only compatible TU3 update
packages are accepted. Older retail packages cannot replace this build.
Updated assets on the installer release are identified by their SHA256
digest and upload time, even when the release tag stays the same.
The gameplay limitations documented below remain.

RELEASE NOTES
Includes Surface scenery, boxes, Klobb assets and native explosive red barrels.
The Klobb loads its CMP150 model/animation package before pickup and checks
missing resources before initialization. Native logs show successful use of
the Klobb after this correction.
Holster copies now remain allocated until native character cleanup; equipping
and dropping weapons changes their visibility instead of repeatedly freeing
models. Manual Reload and third-person/cover blinking corrections are included.

KNOWN LIMITATIONS
The shotgun's cosmetic holster remains disabled; the playable gun is retained.
The latest holster change still needs solo pickup/switching stress testing.
Random bot body/head buttons have an unresolved prior report.
This is a release candidate. See VALIDATION.txt for test coverage and limits.

GET STARTED
Open PDZ Setup.exe. All installer support files are embedded; distribute
only that EXE, README.txt and VALIDATION.txt. The installer temporarily
extracts its support files and removes them when it closes. The installed
game still requires its extracted game assets, runtime files and saves.

Requires 64-bit Windows 10/11 and .NET Framework 4.8. Supply your
US original or US Rev 1 Xbox 360 Perfect Dark Zero ISO, or a clean extraction.
The revision is detected from its verified executable and disc file list.
Other game-disc revisions are not supported. The separate Limited Edition
BONUS disc remains optional. The source ISO is never modified.

With Rev 1, the installer prepares the current TU3-compatible Reinstated
build automatically. No separate title-update package is needed. Rev 1's
bundled map-pack files are retained. This compatibility conversion targets
the supported TU3 executable; it does not run Rev 1's executable directly.
For an original disc, supply Title Update 3 on the game-source page. Its
original content package or ZIP is accepted. Skipping TU3 on an original
disc installs the earlier build without all current features. Original-disc
users still supply their own optional DLC. Additional language sources
remain optional for both revisions.
The installer does not download game discs, title updates, DLC or voices.

Choose a NEW destination folder. Existing installations and saves are not
overwritten. Installation is staged and verified before it is published.
After installation, launch PDZReinstatedTU3.exe directly (or
perfectdarkzerorecomp.exe for the original-disc build). No CMD launcher
is needed. Saves and settings stay in the installed game folder.

NEW-PLAYER DEFAULTS AND OPTIONS
Missions unlock through normal progression. Walking speed defaults to 20%.
Extended Options provides the FOV, input, graphics and gameplay controls,
including cutscene subtitles and the mission timer. Left Shift walks;
use the key editor to change bindings. Delete clears a binding; Escape
asks whether to bind or cancel. Duplicate bindings warn before confirmation.
Resetting bindings asks for confirmation. Middle mouse and wheel actions
are bindable; while aiming, wheel input is reserved for zoom. Editor text
size can be adjusted with its slider or Ctrl+wheel.

SHADERS
Shaders are compiled and cached as scenes and effects are encountered.
Extended Options > Graphics > Shader loading offers Play while shaders
compile. This can reduce compilation pauses, but unfinished effects may
pop in or show temporary purple materials. Other loading stalls remain
possible. Renderer changes can require different cached pipelines.
The Shader cache page offers two choices:
  Use the completed playthrough cache (recommended)
  Build my own cache
The recommended option includes the creator's completed-playthrough shader
data in this installer; no separate download is required. It installs the
shareable shader microcode and Direct3D 12 ROV/RTV and Vulkan pipeline
descriptions into user/cache/shaders/shareable in the game folder.
Your PC still prepares these shaders for its GPU, so the first launch can
take longer. It helps reduce first-use compilation during play; new effects
or other hardware/settings can still require additional work. The cache
is for the TU3 build. Without TU3, the installer uses a fresh cache.
Build my own cache installs no shared shader files. The game builds and
saves its own cache as you play; first-time effects can pause or pop in.
No personal saves, settings, map-load logs or driver-specific binaries are
included. Both choices retain the same game content and normal progression.

INCLUDED TU3 CONTENT
Surface terrain, mission music and required support assets are included
in the installer. The current game build includes the confirmed KSI-74
bayonet recovery and Mission 1 AudioScope cinematic droplets, which clear
when the camera returns to the normal cinematic view.
Native snow is enabled in Missions 4, 6 and 7, with lighter snow in 7.
Custom snow supplements Mission 4; Missions 6/7 use native snow only.
Cold breath includes the marked Mission 7 area exceptions.
Mission 6 uses sparse native cave drips. Added native splash and ripple
emitters are disabled because they interfered with weapon effects.
See VALIDATION.txt for verification scope and remaining checks.

LANGUAGES
Installer text offers English, Japanese, German, French, European Spanish,
Italian, Korean, Traditional Chinese and Latin American Spanish.
Game language sources:
  US retail (En,Fr,Es): English, French, Latin American Spanish.
  Europe (En,Fr,De): German.
  Europe (En,Es,It): European Spanish and Italian.
  Japan/Korea (En,Ja,Zh,Ko): Japanese, Traditional Chinese, Korean text;
    Japanese voices. Chinese/Korean are display options, not voice options.
Supply the matching ISOs or extracted folders. Display and voice pages
share sources. Install the matching files before switching languages in
Extended Options; save and restart. Reload missions after changing voices.

OPTIONAL BONUS DVD
The original Xbox bonus-disc menu application is not ported. The PC viewer
preserves the disc files and plays its videos/audio using bundled VLC.
Launch PDZ Bonus DVD.exe from the installed game folder.
Keyboard: Up/Down selects, Enter plays, M returns, Space pauses, Esc exits
or returns; Left/Right seeks 10 seconds, Home/End jumps to start/end.
XInput: D-pad Up/Down selects, A plays, B returns, Start pauses;
D-pad Left/Right seeks 10 seconds. Physical controller testing is pending.
The seek bar and +/-10-second buttons preserve pause state.

INSTALLER GALLERY AND AUDIO
Music and menu SFX have separate On/Off controls. The comic gallery supports
Pause/Resume, Space and full-screen reading. Click an image to pause/read;
Fit Page, 100%, +/-, Ctrl+wheel and scrolling control the view. Esc returns
to the paused gallery. Replay starts it again.

LICENCES AND CREDITS
PerfectDarkZeroRecomp upstream: https://github.com/nikolaygorb/PerfectDarkZeroRecomp
extract-xiso: https://github.com/XboxDev/extract-xiso
Its licence is embedded with the installer support files as tools/LICENSE.TXT.
VLC 3.0.24 components and licences are installed under bonus/vlc/ when used.
Official binary/source archive: https://download.videolan.org/pub/videolan/vlc/3.0.24/
Existing game/mod licences are included in the installed payload. Artwork
and music are the supplied/chosen materials. No full game disc is bundled.
