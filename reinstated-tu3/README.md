# Reinstated TU3 source backup — v121 installer / v120 game

This snapshot preserves the current TU3 modifications, generated TU3 translation,
tests, installer sources, packaging tools, and the SDK working-tree patch.
It sits alongside the older repository tree so that older work is preserved.
`BASELINES.json` identifies the SDK and game baseline commits. Apply
`compatibility-v16/sdk-working-tree.patch` to that SDK baseline; the separately
included SDK files are additions. A libmspack patch is included for its recorded baseline.

The release assets contain the installer and runtime update. The installer embeds
the reviewed playthrough shader seed as an optional choice. Personal saves,
profiles, live settings, crash dumps, and logs are excluded from this snapshot.
Original game disc inputs, compilers, and compiled build intermediates are not included.

The build scripts preserve the working Windows layout and local paths. A clean
machine needs the matching SDK dependencies, LLVM 20.1.8, Visual Studio tools,
Windows SDK, and legally supplied game/TU3 inputs; regenerate the baseline CMake
build and stock objects before running `tools/build-modded-tu3.py --next`.
This is a source backup, not a verified one-command clean-machine build.

`installer/VALIDATION.txt` records test coverage and outstanding gameplay checks.
v121 fixes updater packaging; its game executable is unchanged from v120.
