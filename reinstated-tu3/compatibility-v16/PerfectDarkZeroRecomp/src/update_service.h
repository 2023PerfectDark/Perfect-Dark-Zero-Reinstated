#pragma once
#include <string>
#include <vector>
namespace pdz_update {
struct Status { std::string phase="idle", message="Check for updates to the modded recomp."; int percent=0, seconds=-1; };
Status Poll();
void Check();
void Download();
void Redownload();
void Wait();
void Restart();
bool Successful();
void Acknowledge();
// Opens the community report board; no report or diagnostic data is uploaded.
bool OpenReportBoard();
std::vector<std::u16string> ReadChangelog();
}
