import Foundation
import UserNotifications

/// Tracks snoozed messages locally and schedules a reminder notification.
/// Gmail has no snooze API, so "un-snooze" (re-add to inbox) happens the next
/// time the app launches after the snooze time passes — see InboxViewModel.
final class SnoozeStore {
    static let shared = SnoozeStore()
    private let key = "snoozed_messages_v1"

    struct Item: Codable { let id: String; let until: Date }

    func add(_ email: Email, until: Date) {
        var items = all()
        items.removeAll { $0.id == email.id }
        items.append(Item(id: email.id, until: until))
        save(items)
        scheduleNotification(email: email, at: until)
    }

    func remove(_ id: String) {
        save(all().filter { $0.id != id })
        UNUserNotificationCenter.current().removePendingNotificationRequests(withIdentifiers: [id])
    }

    /// Ids whose snooze time has elapsed.
    func due(now: Date = Date()) -> [String] {
        all().filter { $0.until <= now }.map { $0.id }
    }

    func all() -> [Item] {
        guard let data = UserDefaults.standard.data(forKey: key),
              let items = try? JSONDecoder().decode([Item].self, from: data) else { return [] }
        return items
    }

    private func save(_ items: [Item]) {
        UserDefaults.standard.set(try? JSONEncoder().encode(items), forKey: key)
    }

    func requestAuthorization() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    private func scheduleNotification(email: Email, at date: Date) {
        let content = UNMutableNotificationContent()
        content.title = "Back in your inbox"
        content.body = "\(email.from): \(email.subject)"
        content.sound = .default

        let comps = Calendar.current.dateComponents([.year, .month, .day, .hour, .minute], from: date)
        let trigger = UNCalendarNotificationTrigger(dateMatching: comps, repeats: false)
        let request = UNNotificationRequest(identifier: email.id, content: content, trigger: trigger)
        UNUserNotificationCenter.current().add(request)
    }
}
