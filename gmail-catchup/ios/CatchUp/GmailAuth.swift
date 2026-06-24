import Foundation
import AuthenticationServices
import CryptoKit
import UIKit
import Observation

enum AuthError: LocalizedError {
    case notConfigured, cancelled, noCode, notSignedIn, server(String)
    var errorDescription: String? {
        switch self {
        case .notConfigured: return "Add your Google client ID in Config.swift first."
        case .cancelled: return "Sign-in was cancelled."
        case .noCode: return "Google didn't return an authorization code."
        case .notSignedIn: return "You're signed out. Please sign in again."
        case .server(let m): return m
        }
    }
}

/// OAuth 2.0 with PKCE against Google, using ASWebAuthenticationSession.
/// No third-party SDKs: the access token is held in memory and refreshed on
/// demand; the long-lived refresh token lives in the Keychain.
@Observable
final class GmailAuth: NSObject {
    private(set) var isSignedIn = false
    var userEmail: String?

    private var accessToken: String?
    private var accessTokenExpiry: Date?
    private var webAuthSession: ASWebAuthenticationSession?

    private let keychain = Keychain(service: "gg.recess.catchup")
    private let refreshKey = "google_refresh_token"
    private let authEndpoint = URL(string: "https://accounts.google.com/o/oauth2/v2/auth")!
    private let tokenEndpoint = URL(string: "https://oauth2.googleapis.com/token")!

    override init() {
        super.init()
        isSignedIn = keychain.get(refreshKey) != nil
    }

    // MARK: Sign in

    @MainActor
    func signIn() async throws {
        guard Config.isConfigured else { throw AuthError.notConfigured }

        let verifier = Self.randomCodeVerifier()
        let challenge = Self.codeChallenge(for: verifier)

        var comps = URLComponents(url: authEndpoint, resolvingAgainstBaseURL: false)!
        comps.queryItems = [
            .init(name: "client_id", value: Config.googleClientID),
            .init(name: "redirect_uri", value: Config.redirectURI),
            .init(name: "response_type", value: "code"),
            .init(name: "scope", value: Config.scopes.joined(separator: " ")),
            .init(name: "code_challenge", value: challenge),
            .init(name: "code_challenge_method", value: "S256"),
            .init(name: "access_type", value: "offline"),
            .init(name: "prompt", value: "consent"),
        ]

        let callbackURL: URL = try await withCheckedThrowingContinuation { cont in
            let session = ASWebAuthenticationSession(
                url: comps.url!,
                callbackURLScheme: Config.redirectScheme
            ) { url, error in
                if let url { cont.resume(returning: url) }
                else { cont.resume(throwing: error ?? AuthError.cancelled) }
            }
            session.presentationContextProvider = self
            session.prefersEphemeralWebBrowserSession = false
            self.webAuthSession = session
            session.start()
        }

        guard let code = URLComponents(url: callbackURL, resolvingAgainstBaseURL: false)?
            .queryItems?.first(where: { $0.name == "code" })?.value else {
            throw AuthError.noCode
        }

        try await exchangeCode(code, verifier: verifier)
        try? await fetchProfile()
        isSignedIn = true
    }

    func signOut() {
        keychain.delete(refreshKey)
        accessToken = nil
        accessTokenExpiry = nil
        userEmail = nil
        isSignedIn = false
    }

    // MARK: Tokens

    /// Returns a valid access token, refreshing if needed.
    func validAccessToken() async throws -> String {
        if let token = accessToken, let exp = accessTokenExpiry, exp > Date().addingTimeInterval(60) {
            return token
        }
        return try await refresh()
    }

    private func exchangeCode(_ code: String, verifier: String) async throws {
        let token = try await postToken([
            "client_id": Config.googleClientID,
            "code": code,
            "code_verifier": verifier,
            "grant_type": "authorization_code",
            "redirect_uri": Config.redirectURI,
        ])
        apply(token)
        if let refresh = token.refresh_token { keychain.set(refresh, for: refreshKey) }
    }

    private func refresh() async throws -> String {
        guard let refreshToken = keychain.get(refreshKey) else { throw AuthError.notSignedIn }
        do {
            let token = try await postToken([
                "client_id": Config.googleClientID,
                "refresh_token": refreshToken,
                "grant_type": "refresh_token",
            ])
            apply(token)
            return token.access_token
        } catch {
            // Refresh token revoked/expired: force re-auth.
            await MainActor.run { self.signOut() }
            throw AuthError.notSignedIn
        }
    }

    private func postToken(_ body: [String: String]) async throws -> TokenResponse {
        var req = URLRequest(url: tokenEndpoint)
        req.httpMethod = "POST"
        req.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        req.httpBody = Self.formEncode(body)
        let (data, resp) = try await URLSession.shared.data(for: req)
        guard let http = resp as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            throw AuthError.server(String(data: data, encoding: .utf8) ?? "Token request failed")
        }
        return try JSONDecoder().decode(TokenResponse.self, from: data)
    }

    private func apply(_ t: TokenResponse) {
        accessToken = t.access_token
        accessTokenExpiry = Date().addingTimeInterval(TimeInterval(t.expires_in))
    }

    func fetchProfile() async throws {
        let token = try await validAccessToken()
        var req = URLRequest(url: URL(string: "https://gmail.googleapis.com/gmail/v1/users/me/profile")!)
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        let (data, _) = try await URLSession.shared.data(for: req)
        struct Profile: Decodable { let emailAddress: String }
        let profile = try JSONDecoder().decode(Profile.self, from: data)
        await MainActor.run { self.userEmail = profile.emailAddress }
    }

    // MARK: PKCE helpers

    private static func randomCodeVerifier() -> String {
        var bytes = [UInt8](repeating: 0, count: 32)
        _ = SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes)
        return Data(bytes).base64URLEncoded()
    }

    private static func codeChallenge(for verifier: String) -> String {
        Data(SHA256.hash(data: Data(verifier.utf8))).base64URLEncoded()
    }

    private static func formEncode(_ dict: [String: String]) -> Data {
        dict.map { key, value in
            let v = value.addingPercentEncoding(withAllowedCharacters: .urlFormAllowed) ?? value
            return "\(key)=\(v)"
        }
        .joined(separator: "&")
        .data(using: .utf8)!
    }
}

extension GmailAuth: ASWebAuthenticationPresentationContextProviding {
    func presentationAnchor(for session: ASWebAuthenticationSession) -> ASPresentationAnchor {
        let scene = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .first { $0.activationState == .foregroundActive }
        return scene?.keyWindow ?? ASPresentationAnchor()
    }
}

struct TokenResponse: Decodable {
    let access_token: String
    let expires_in: Int
    let refresh_token: String?
}

extension Data {
    func base64URLEncoded() -> String {
        base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
    }
}

extension CharacterSet {
    /// Unreserved set for application/x-www-form-urlencoded values.
    static let urlFormAllowed: CharacterSet = {
        var set = CharacterSet.alphanumerics
        set.insert(charactersIn: "-._~")
        return set
    }()
}
