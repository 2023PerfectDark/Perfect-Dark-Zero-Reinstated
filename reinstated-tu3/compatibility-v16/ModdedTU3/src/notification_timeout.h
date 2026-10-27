#pragma once
namespace pdz_ui {
struct NotificationTimeout {
  double shownAt = 0;
  bool active = false;
  bool Hidden(double now, bool visible) {
    if (!visible) { active = false; return false; }
    if (!active) { shownAt = now; active = true; }
    return now - shownAt >= 5.0;
  }
};
}
