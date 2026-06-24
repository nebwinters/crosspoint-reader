import SwiftUI

@main
struct CatchUpApp: App {
    @State private var auth = GmailAuth()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(auth)
                .tint(Color(red: 0.04, green: 0.52, blue: 1.00))
                .onAppear { SnoozeStore.shared.requestAuthorization() }
        }
    }
}
