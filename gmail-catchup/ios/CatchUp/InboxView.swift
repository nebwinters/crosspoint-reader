import SwiftUI

struct InboxView: View {
    let auth: GmailAuth
    @State private var model: InboxViewModel

    init(auth: GmailAuth) {
        self.auth = auth
        _model = State(initialValue: InboxViewModel(service: GmailService(auth: auth)))
    }

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            list
            ComposeBar { to, body in await model.send(to: to, body: body) }
        }
        .background(Color(.systemGroupedBackground))
        .overlay(alignment: .bottom) { toast }
        .task { await model.load() }
        .refreshable { await model.load() }
        .alert("Something went wrong",
               isPresented: Binding(get: { model.errorMessage != nil },
                                    set: { if !$0 { model.errorMessage = nil } })) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(model.errorMessage ?? "")
        }
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 2) {
                Text("Catch-Up").font(.system(size: 28, weight: .heavy))
                Text(model.emails.isEmpty ? " " : "\(model.emails.count) to triage")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
            Spacer()
            Menu {
                if let email = auth.userEmail { Text(email) }
                Button("Refresh") { Task { await model.load() } }
                Button("Sign out", role: .destructive) { auth.signOut() }
            } label: {
                Image(systemName: "person.crop.circle").font(.title2)
            }
        }
        .padding(.horizontal, 18)
        .padding(.top, 8)
        .padding(.bottom, 10)
    }

    @ViewBuilder
    private var list: some View {
        if model.isLoading && model.emails.isEmpty {
            Spacer(); ProgressView("Loading inbox…"); Spacer()
        } else if model.emails.isEmpty {
            emptyState
        } else {
            ScrollView {
                LazyVStack(spacing: 10) {
                    ForEach(model.emails) { email in
                        EmailCardView(email: email) { kind in
                            model.perform(kind, on: email)
                        }
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 12)
            }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 10) {
            Spacer()
            Text("🎉").font(.system(size: 52))
            Text("Inbox zero").font(.title2.weight(.bold))
            Text("Nothing left to triage.").foregroundStyle(.secondary)
            Button("Refresh") { Task { await model.load() } }
                .buttonStyle(.bordered)
                .padding(.top, 8)
            Spacer()
        }
    }

    @ViewBuilder
    private var toast: some View {
        if let toast = model.toast {
            HStack(spacing: 12) {
                if let kind = toast.kind {
                    Circle().fill(kind.tint).frame(width: 9, height: 9)
                }
                Text(toast.text).fontWeight(.semibold)
                if toast.showsUndo {
                    Button("Undo") { model.undo() }
                        .fontWeight(.bold)
                }
            }
            .foregroundStyle(.white)
            .padding(.horizontal, 16).padding(.vertical, 11)
            .background(Color(.darkGray).opacity(0.95), in: Capsule())
            .padding(.bottom, 86)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }
}
