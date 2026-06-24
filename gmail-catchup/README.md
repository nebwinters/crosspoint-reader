# Catch-Up

A Slack-catch-up-style swipe interface for Gmail.

- **`web-prototype/`** — a single self-contained HTML file used to refine the
  swipe/hold/snooze feel quickly in a browser. `index.html` ships with mock
  data; `index.local.html` (gitignored) can be filled with real inbox content
  for testing.
- **`ios/`** — the real native **SwiftUI** app with live Google OAuth and the
  Gmail REST API. See [`ios/README.md`](ios/README.md) for setup + install
  steps.

> Note: this lives inside the `crosspoint-reader` repo for convenience but is an
> entirely separate project from the e-reader firmware.
