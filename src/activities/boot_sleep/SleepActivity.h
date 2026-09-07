#pragma once
#include "activities/Activity.h"

// Dashboard auto-sync marker on the SD root: holds the URL that enterDeepSleep()
// downloads into /sleep.bmp (see main.cpp). Its presence also makes SleepActivity
// render /sleep.bmp for non-quick-resume sleeps, without rewriting the user's
// configured sleep screen mode.
inline constexpr char DASHBOARD_URL_FILE[] = "/dashboard.url";

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap) const;
  void renderLastScreenSleepScreen() const;
  void renderBlankSleepScreen() const;

  bool fromTimeout = false;
};
