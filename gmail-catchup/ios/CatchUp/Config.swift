import Foundation

/// One-time setup: paste your Google OAuth *iOS* client ID below.
/// See ../README.md for how to create it (takes ~3 minutes, free).
enum Config {
    /// Looks like: 1234567890-abcdefg.apps.googleusercontent.com
    static let googleClientID = "PASTE_YOUR_CLIENT_ID.apps.googleusercontent.com"

    /// Google's iOS OAuth clients use the *reversed* client ID as the redirect
    /// scheme. Derived automatically — no need to edit.
    static var redirectScheme: String {
        let id = googleClientID.replacingOccurrences(of: ".apps.googleusercontent.com", with: "")
        return "com.googleusercontent.apps.\(id)"
    }
    static var redirectURI: String { "\(redirectScheme):/oauth2redirect" }

    /// gmail.modify  → read inbox + change labels (archive / unread)
    /// gmail.send    → send mail from the compose bar
    static let scopes = [
        "https://www.googleapis.com/auth/gmail.modify",
        "https://www.googleapis.com/auth/gmail.send",
    ]

    static var isConfigured: Bool { !googleClientID.hasPrefix("PASTE_") }
}
