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

The default "CrossPoint SLEEPING" screen means no usable image is on the card.
Failures are logged over serial only, so check in this order:

1. The reader is running a build that includes this feature (the version on
   the web Home page ends in a branch name and commit hash).
2. `dashboard.url` is in the SD root, starts with `http`, and is under 250
   characters.
3. A saved Wi-Fi network is in range. The sync gives up after 7 seconds.
4. The URL is reachable and returns a BMP. Open it in a phone browser to check.
5. Start **File Transfer** and inspect `sleep.bmp` in the SD root via the web
   File Manager. Its size and whether it opens as an image tells you whether a
   download ever succeeded.
