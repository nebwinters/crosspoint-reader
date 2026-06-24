# Catch-Up — Gmail swipe triage (iOS)

A native SwiftUI app that turns your Gmail inbox into a Slack-catch-up-style
swipe deck:

- **Swipe left** → mark unread
- **Press & hold, then swipe left** → snooze (back tomorrow 9 AM)
- **Swipe right** → archive
- **Compose bar** at the bottom to fire off a quick email
- **Undo** toast after every action

No third-party SDKs — OAuth is done with `ASWebAuthenticationSession` + PKCE,
and everything talks to the Gmail REST API directly.

---

## What you need

- A **Mac with Xcode 16+**
- An **iPhone** running iOS 17+ (and a Lightning/USB-C cable)
- A free **Google account** (your Gmail)
- *(Optional)* an Apple Developer account. A free Apple ID works for installing
  on your own phone — the app just expires after 7 days and you re-run it from
  Xcode to refresh it. The $99/yr program gives a 1-year signing certificate.

---

## 1. Create a Google OAuth client (~3 min, free)

1. Go to <https://console.cloud.google.com/> and create a new project
   (e.g. "Catch-Up").
2. **APIs & Services → Library** → search **Gmail API** → **Enable**.
3. **APIs & Services → OAuth consent screen**:
   - User type: **External** → Create
   - Fill app name + your email; **Save and continue**
   - **Scopes**: add `.../auth/gmail.modify` and `.../auth/gmail.send`
   - **Test users**: add your own Gmail address → Save
     *(While the app is in "testing" mode only test users can sign in — that's
     all you need for a personal app.)*
4. **APIs & Services → Credentials → Create Credentials → OAuth client ID**:
   - Application type: **iOS**
   - Bundle ID: `gg.recess.catchup`
     *(must match `PRODUCT_BUNDLE_IDENTIFIER` in the Xcode project — change both
     together if you want a different one)*
   - Create → copy the **Client ID** (ends in `.apps.googleusercontent.com`)

## 2. Paste the client ID

Open `CatchUp/Config.swift` and replace the placeholder:

```swift
static let googleClientID = "1234567890-abcdef.apps.googleusercontent.com"
```

The redirect URI is derived automatically from this — nothing else to set.
(With `ASWebAuthenticationSession` the redirect scheme does **not** need to be
registered in Info.plist.)

## 3. Run it on your phone

1. Open `CatchUp.xcodeproj` in Xcode.
2. Select the **CatchUp** target → **Signing & Capabilities** → pick your
   **Team** (your Apple ID; add it in Xcode → Settings → Accounts if needed).
   Xcode will manage the provisioning profile automatically.
3. Plug in your iPhone and select it as the run destination.
4. Press **▶ Run**. First launch on the device: open
   **Settings → General → VPN & Device Management** on the phone and **trust**
   your developer certificate.
5. Tap **Sign in with Google**, approve the scopes, and you're triaging.

---

## How each action maps to Gmail

| Gesture | Gmail API call |
|---|---|
| Archive (right) | `messages.modify` remove `INBOX` |
| Unread (left) | `messages.modify` add `UNREAD` |
| Snooze (hold+left) | `messages.modify` remove `INBOX` + local reminder |
| Undo | reverses the label change |
| Send | `messages.send` (RFC 822, base64url) |

### Snooze caveat (read this)

Gmail's public API has **no snooze endpoint**. So snooze here:
1. removes the message from your inbox (it still lives in All Mail),
2. schedules a local notification for the return time, and
3. re-adds the `INBOX` label **the next time you open the app** after that time
   passes (`InboxViewModel.processDueSnoozes()`).

True server-side timed re-delivery (return-to-inbox even if the app never
opens) would need a small backend or a Google Apps Script trigger. Out of scope
for v1; easy to add later.

---

## Project layout

```
CatchUp/
  CatchUpApp.swift      app entry, injects GmailAuth
  ContentView.swift     sign-in vs inbox switch
  InboxView.swift       header + scroll list + compose + toast
  EmailCardView.swift   the swipe/hold/snooze gesture card
  ComposeBar.swift      bottom compose UI
  InboxViewModel.swift  state, optimistic actions, undo
  GmailAuth.swift       OAuth PKCE + token refresh
  GmailService.swift    Gmail REST calls
  SnoozeStore.swift     local snooze tracking + notifications
  Keychain.swift        refresh-token storage
  Models.swift          Email model + API decoding
  Config.swift          ← your client ID goes here
```

The interaction was prototyped first in `../web-prototype/` — open that HTML in
a browser to tweak feel without a build.
