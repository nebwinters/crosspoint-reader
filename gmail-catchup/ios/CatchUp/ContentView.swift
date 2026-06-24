import SwiftUI

struct ContentView: View {
    @Environment(GmailAuth.self) private var auth

    var body: some View {
        if auth.isSignedIn {
            InboxView(auth: auth)
        } else {
            SignInView(auth: auth)
        }
    }
}

struct SignInView: View {
    let auth: GmailAuth
    @State private var error: String?
    @State private var working = false

    var body: some View {
        VStack(spacing: 18) {
            Spacer()
            Image(systemName: "tray.full.fill")
                .font(.system(size: 64))
                .foregroundStyle(.tint)
            Text("Catch-Up")
                .font(.system(size: 34, weight: .heavy, design: .default))
            Text("Swipe through your inbox.\nLeft to unread, hold-left to snooze, right to archive.")
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
                .padding(.horizontal, 32)
            Spacer()

            if !Config.isConfigured {
                Label("Add your Google client ID in Config.swift", systemImage: "exclamationmark.triangle.fill")
                    .font(.footnote)
                    .foregroundStyle(.orange)
                    .padding(.bottom, 4)
            }

            Button {
                Task { await signIn() }
            } label: {
                HStack {
                    if working { ProgressView().tint(.white) }
                    Text("Sign in with Google")
                        .fontWeight(.semibold)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 15)
                .background(.tint, in: RoundedRectangle(cornerRadius: 16))
                .foregroundStyle(.white)
            }
            .disabled(working || !Config.isConfigured)
            .padding(.horizontal, 24)
            .padding(.bottom, 40)

            if let error {
                Text(error).font(.footnote).foregroundStyle(.red).padding(.bottom, 12)
            }
        }
    }

    private func signIn() async {
        working = true
        defer { working = false }
        do { try await auth.signIn() }
        catch { self.error = (error as? LocalizedError)?.errorDescription ?? error.localizedDescription }
    }
}
