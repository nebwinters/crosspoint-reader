import Foundation
import Observation

struct ToastInfo: Equatable {
    let text: String
    let kind: ActionKind?
    let showsUndo: Bool
}

@Observable
@MainActor
final class InboxViewModel {
    var emails: [Email] = []
    var isLoading = false
    var errorMessage: String?
    var toast: ToastInfo?

    private let service: GmailService
    private var lastAction: (email: Email, index: Int, kind: ActionKind)?
    private var toastTask: Task<Void, Never>?

    init(service: GmailService) { self.service = service }

    // MARK: Loading

    func load() async {
        isLoading = true
        errorMessage = nil
        do {
            await processDueSnoozes()
            emails = try await service.listInbox()
        } catch {
            errorMessage = friendly(error)
        }
        isLoading = false
    }

    /// Re-file any snoozes whose time has passed (best-effort, runs at launch).
    func processDueSnoozes() async {
        for id in SnoozeStore.shared.due() {
            do {
                try await service.addInbox(id)
                SnoozeStore.shared.remove(id)
            } catch { /* leave it; retry next launch */ }
        }
    }

    // MARK: Swipe actions (optimistic + undo)

    func perform(_ kind: ActionKind, on email: Email) {
        guard let index = emails.firstIndex(of: email) else { return }

        var stored = email
        if kind == .unread { stored.isUnread = true }
        lastAction = (stored, index, kind)
        emails.remove(at: index)
        showToast(ToastInfo(text: kind.label, kind: kind, showsUndo: true))

        Task {
            do {
                switch kind {
                case .archive: try await service.archive(email)
                case .unread: try await service.markUnread(email)
                case .snooze:
                    try await service.snooze(email)
                    SnoozeStore.shared.add(email, until: Self.tomorrowMorning())
                }
            } catch {
                revertLastAction(error: error)
            }
        }
    }

    func undo() {
        guard let last = lastAction else { return }
        lastAction = nil
        toast = nil
        // Put it back optimistically.
        emails.insert(last.email, at: min(last.index, emails.count))

        Task {
            do {
                switch last.kind {
                case .archive, .snooze:
                    try await service.unarchive(last.email)
                    if last.kind == .snooze { SnoozeStore.shared.remove(last.email.id) }
                case .unread:
                    try await service.markRead(last.email)
                }
            } catch {
                errorMessage = friendly(error)
            }
        }
    }

    private func revertLastAction(error: Error) {
        guard let last = lastAction else { return }
        emails.insert(last.email, at: min(last.index, emails.count))
        lastAction = nil
        errorMessage = friendly(error)
    }

    // MARK: Compose

    func send(to: String, body: String) async -> Bool {
        let subject = body.split(separator: "\n").first.map(String.init) ?? "(no subject)"
        do {
            try await service.sendMail(to: to, subject: String(subject.prefix(80)), body: body)
            showToast(ToastInfo(text: "Sent", kind: nil, showsUndo: false))
            return true
        } catch {
            errorMessage = friendly(error)
            return false
        }
    }

    // MARK: Helpers

    private func showToast(_ info: ToastInfo) {
        toast = info
        toastTask?.cancel()
        toastTask = Task {
            try? await Task.sleep(for: .seconds(3.2))
            if !Task.isCancelled { toast = nil }
        }
    }

    private func friendly(_ error: Error) -> String {
        (error as? LocalizedError)?.errorDescription ?? error.localizedDescription
    }

    static func tomorrowMorning() -> Date {
        let cal = Calendar.current
        let tomorrow = cal.date(byAdding: .day, value: 1, to: Date()) ?? Date()
        return cal.date(bySettingHour: 9, minute: 0, second: 0, of: tomorrow) ?? tomorrow
    }
}
