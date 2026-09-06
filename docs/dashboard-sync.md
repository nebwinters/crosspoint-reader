# Dashboard Sync (Sleep Screen from a URL)

CrossPoint can fetch an image from a URL and show it as the sleep screen, so a
reader left on a desk or a charger doubles as an always-on e-ink dashboard.

## Setup

1. Save a Wi-Fi network on the reader (**Settings > System > Wi-Fi Networks**)
   and connect to it once. The sync reuses the last connected network.
2. Create a file named `dashboard.url` in the root of the SD card:

   ```text
   https://example.com/dashboard.bmp
   30
   ```

   - **Line 1**: the URL of the image. `http://` and `https://` are both
     supported; HTTPS is verified against the built-in certificate bundle.
   - **Line 2** (optional): refresh interval in minutes while USB-powered.
     Defaults to `60`. Allowed range is 5 to 1440. `0` disables the periodic
     refresh and only syncs when the reader goes to sleep.

3. Put the reader to sleep. The sleep screen paints first, then the reader
   connects and downloads. If a new image arrived, the screen repaints with it.

The presence of `dashboard.url` is the on/off switch. While it exists, the sleep
screen always shows `/sleep.bmp`; the configured sleep screen mode is left
untouched. Delete the file to go back to the normal sleep screen; edit it to
re-point the source. No rebuild is needed.

## Image requirements

The URL must serve an actual **BMP** file. The sleep screen renderer accepts
uncompressed BMPs at 1, 4, 8, or 24 bits per pixel, or 32-bit with bitfields.
For the X4 use 480x800 pixels. PNG, JPEG, or an HTML page will be rejected and
the reader falls back to the default sleep screen.

A failed download never replaces the previous image: the fetch goes to a
temporary file and is swapped in only on success.

## How refresh works

- **At every sleep entry** the reader fetches the image after painting the
  sleep screen, and repaints if the image changed. Quick Resume sleeps skip the
  sync, since they show the saved session frame instead.
- **While plugged into USB**, the reader also wakes itself on a timer every
  `refresh interval` minutes, fetches again, repaints, and goes back to sleep.
  No UI is shown during these refreshes, and pressing the power button still
  wakes the reader normally.
- **On battery**, the periodic refresh is not possible. The hardware cuts power
  to the MCU entirely during sleep, so nothing can wake it except the power
  button. The image still refreshes each time you put the reader to sleep.

## Troubleshooting

After every sync the reader overwrites `dashboard.status` in the SD root with
the outcome, so you can diagnose from a phone via the web File Manager:

```text
firmware: 1.3.0-dev-master-abc1234
result: download-failed
detail: download error 1
network: HomeWifi
uptime_s: 14
```

| result | meaning |
|--------|---------|
| `ok` | Image fetched and swapped into `sleep.bmp`. `detail` is the URL. |
| `wifi-failed` | No saved network, none in range, or the join timed out (`detail` says which). |
| `download-failed` | Joined Wi-Fi but the fetch failed. Error 1 = HTTP/TLS, 2 = SD write, 3 = aborted. |
| `rename-failed` | Downloaded, but the SD swap into `sleep.bmp` failed. |
| `update-installed` | A newer fork release was flashed; `detail` is its version. Next boot resumes the dashboard. |
| `update-failed` | The release download or flash failed; the current firmware stays. |

A failed sync keeps the previous `sleep.bmp`, so "the image didn't change" and
"the sync failed" look the same on screen. The status file tells them apart.

The default "CrossPoint SLEEPING" screen means no usable image is on the card.
If there is no status file at all, the sync never ran, so check in this order:

1. The reader is running a build that includes this feature (the version on
   the web Home page ends in a branch name and commit hash).
2. `dashboard.url` is in the SD root, starts with `http`, and is under 250
   characters.
3. A saved Wi-Fi network is in range. Each join gets 12 seconds and one retry
   (about 36 seconds worst case), so a weak signal mostly works but a dead one
   does not stall sleep for long.
4. The URL is reachable and returns a BMP. Open it in a phone browser to check.
5. Start **File Transfer** and inspect `sleep.bmp` in the SD root via the web
   File Manager. Its size and whether it opens as an image tells you whether a
   download ever succeeded.

## Unattended firmware updates

Readers running this fork update themselves. Every push to `master` builds a
release (`.github/workflows/fork-release.yml`) versioned `MAJOR.MINOR.<run>` and
publishes `firmware.bin` under the fork's GitHub releases. The over-the-air
updater points at that feed, and dashboard refreshes double as the update check:

- Roughly every 6 hours of refreshes, while the network is up for the image
  fetch, the reader asks GitHub for the latest release.
- If it is newer than the running build it downloads and flashes it, then
  restarts straight back into the dashboard sleep loop. No screen is shown.
- Failures leave the current firmware in place and are retried at the next
  check. A user-initiated sleep never waits on an update; only the automatic
  refreshes do.

Settings > System > Check for updates uses the same fork feed, so a manual
check is still possible. The base version lives in `platformio.ini`; bump
`MAJOR.MINOR` there when merging upstream so fork releases stay ahead of the
build already on the device.

## Battery refresh (opt-in)

Add a line containing `battery` to `dashboard.url` and the reader keeps
refreshing on battery too. Instead of powering off it parks in light sleep
between fetches, so the timer can fire. This keeps the whole board powered
while asleep and will noticeably shorten battery life; the exact cost depends
on the board's idle draw, which has not been measured. The power button still
wakes the reader normally. Without the line, battery behavior is unchanged:
one refresh at each sleep entry.

## Choosing networks

The sync scans and joins the strongest saved network in range, trying up to
three, and always falls back to the last connected network even if the scan
missed it. That covers an idle phone hotspot, which stops beaconing until a
client tries to join. Save both your home network and your hotspot on the
reader and it will use whichever is available.
