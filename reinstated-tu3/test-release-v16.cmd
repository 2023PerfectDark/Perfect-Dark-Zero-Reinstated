@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "TEMP=%~dp0release-v16-checks"
set "TMP=%TEMP%"
echo Checking input_logic
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\input_logic_test.cpp -o release-v16-checks\input_logic.exe
if errorlevel 1 exit /b 1
release-v16-checks\input_logic.exe
if errorlevel 1 exit /b 1
echo Checking profile_store
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\profile_store_test.cpp -o release-v16-checks\profile_store.exe
if errorlevel 1 exit /b 1
release-v16-checks\profile_store.exe
if errorlevel 1 exit /b 1
echo Checking virtual_coop_input
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\virtual_coop_input_test.cpp -o release-v16-checks\virtual_coop_input.exe
if errorlevel 1 exit /b 1
release-v16-checks\virtual_coop_input.exe
if errorlevel 1 exit /b 1
echo Checking breathing_motion
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\breathing_motion_test.cpp -o release-v16-checks\breathing_motion.exe
if errorlevel 1 exit /b 1
release-v16-checks\breathing_motion.exe
if errorlevel 1 exit /b 1
echo Checking campaign_logic
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\campaign_logic_test.cpp -o release-v16-checks\campaign_logic.exe
if errorlevel 1 exit /b 1
release-v16-checks\campaign_logic.exe
if errorlevel 1 exit /b 1
echo Checking cheat_progress
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\cheat_progress_test.cpp -o release-v16-checks\cheat_progress.exe
if errorlevel 1 exit /b 1
release-v16-checks\cheat_progress.exe
if errorlevel 1 exit /b 1
echo Checking debug_position
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\debug_position_test.cpp -o release-v16-checks\debug_position.exe
if errorlevel 1 exit /b 1
release-v16-checks\debug_position.exe
if errorlevel 1 exit /b 1
echo Checking disco_dwell
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\disco_dwell_test.cpp -o release-v16-checks\disco_dwell.exe
if errorlevel 1 exit /b 1
release-v16-checks\disco_dwell.exe
if errorlevel 1 exit /b 1
echo Checking guard_wave
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\guard_wave_test.cpp -o release-v16-checks\guard_wave.exe
if errorlevel 1 exit /b 1
release-v16-checks\guard_wave.exe
if errorlevel 1 exit /b 1
echo Checking holster_logic
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\holster_logic_test.cpp -o release-v16-checks\holster_logic.exe
if errorlevel 1 exit /b 1
release-v16-checks\holster_logic.exe
if errorlevel 1 exit /b 1
echo Checking inspection_motion
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\inspection_motion_test.cpp -o release-v16-checks\inspection_motion.exe
if errorlevel 1 exit /b 1
release-v16-checks\inspection_motion.exe
if errorlevel 1 exit /b 1
echo Checking klobb_reload_policy
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\klobb_reload_policy_test.cpp -o release-v16-checks\klobb_reload_policy.exe
if errorlevel 1 exit /b 1
release-v16-checks\klobb_reload_policy.exe
if errorlevel 1 exit /b 1
echo Checking mission_summary
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\mission_summary_test.cpp -o release-v16-checks\mission_summary.exe
if errorlevel 1 exit /b 1
release-v16-checks\mission_summary.exe
if errorlevel 1 exit /b 1
echo Checking mission_weapon_policy
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\mission_weapon_policy_test.cpp -o release-v16-checks\mission_weapon_policy.exe
if errorlevel 1 exit /b 1
release-v16-checks\mission_weapon_policy.exe
if errorlevel 1 exit /b 1
echo Checking mp_progression_policy
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\mp_progression_policy_test.cpp -o release-v16-checks\mp_progression_policy.exe
if errorlevel 1 exit /b 1
release-v16-checks\mp_progression_policy.exe
if errorlevel 1 exit /b 1
echo Checking nightclub_suspicion
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\nightclub_suspicion_test.cpp -o release-v16-checks\nightclub_suspicion.exe
if errorlevel 1 exit /b 1
release-v16-checks\nightclub_suspicion.exe
if errorlevel 1 exit /b 1
echo Checking npc_placement
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\npc_placement_test.cpp -o release-v16-checks\npc_placement.exe
if errorlevel 1 exit /b 1
release-v16-checks\npc_placement.exe
if errorlevel 1 exit /b 1
echo Checking perfect_dark_zero_mode
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\perfect_dark_zero_mode_test.cpp -o release-v16-checks\perfect_dark_zero_mode.exe
if errorlevel 1 exit /b 1
release-v16-checks\perfect_dark_zero_mode.exe
if errorlevel 1 exit /b 1
echo Checking physics_timing
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\physics_timing_test.cpp -o release-v16-checks\physics_timing.exe
if errorlevel 1 exit /b 1
release-v16-checks\physics_timing.exe
if errorlevel 1 exit /b 1
echo Checking reload_timing
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\reload_timing_test.cpp -o release-v16-checks\reload_timing.exe
if errorlevel 1 exit /b 1
release-v16-checks\reload_timing.exe
if errorlevel 1 exit /b 1
echo Checking surface_camera_ground
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\surface_camera_ground_test.cpp -o release-v16-checks\surface_camera_ground.exe
if errorlevel 1 exit /b 1
release-v16-checks\surface_camera_ground.exe
if errorlevel 1 exit /b 1
echo Checking surface_mission
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\surface_mission_test.cpp -o release-v16-checks\surface_mission.exe
if errorlevel 1 exit /b 1
release-v16-checks\surface_mission.exe
if errorlevel 1 exit /b 1
echo Checking coop_ally_logic
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\coop_ally_logic_test.cpp -o release-v16-checks\coop_ally_logic.exe
if errorlevel 1 exit /b 1
release-v16-checks\coop_ally_logic.exe
if errorlevel 1 exit /b 1
echo Checking coop_sync_logic
"%~dp0clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin\clang++.exe" -std=c++23 -Icompatibility-v16\sdk\include compatibility-v16\PerfectDarkZeroRecomp\tests\coop_sync_logic_test.cpp -o release-v16-checks\coop_sync_logic.exe
if errorlevel 1 exit /b 1
release-v16-checks\coop_sync_logic.exe
if errorlevel 1 exit /b 1
call test-notification-timeout.cmd
if errorlevel 1 exit /b 1
echo ALL CHECKS PASSED