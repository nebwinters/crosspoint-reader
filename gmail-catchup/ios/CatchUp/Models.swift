import Foundation
import SwiftUI

/// A triage action a swipe can produce.
enum ActionKind: Equatable {
    case archive
    case unread
    case snooze

    var label: String {
        switch self {
        case .archive: return "Archived"
        case .unread: return "Marked unread"
        case .snooze: return "Snoozed"
        }
    }

    var tint: Color {
        switch self {
        case .archive: return Color(red: 0.20, green: 0.78, blue: 0.35) // green
        case .unread: return Color(red: 0.04, green: 0.52, blue: 1.00)  // blue
        case .snooze: return Color(red: 0.37, green: 0.36, blue: 0.90)  // indigo
        }
    }

    var systemImage: String {
        switch self {
        case .archive: return "archivebox.fill"
        case .unread: return "envelope.badge.fill"
        case .snooze: return "moon.fill"
        }
    }
}

/// A single inbox message, flattened from the Gmail API metadata response.
struct Email: Identifiable, Equatable {
    let id: String           // Gmail message id
    let threadId: String
    var from: String         // display name (falls back to email)
    var fromEmail: String
    var subject: String
    var snippet: String
    var date: Date
    var isUnread: Bool

    var initials: String {
        let parts = from.split(separator: " ").filter { !$0.isEmpty }
        let first = parts.first?.first.map(String.init) ?? "?"
        let second = parts.count > 1 ? (parts[1].first.map(String.init) ?? "") : ""
        return (first + second).uppercased()
    }

    var timeLabel: String {
        let cal = Calendar.current
        let f = DateFormatter()
        if cal.isDateInToday(date) {
            f.dateFormat = "h:mm a"
        } else if cal.isDateInYesterday(date) {
            return "Yesterday"
        } else if let days = cal.dateComponents([.day], from: date, to: Date()).day, days < 7 {
            f.dateFormat = "EEE"
        } else {
            f.dateFormat = "MMM d"
        }
        return f.string(from: date)
    }

    /// Deterministic avatar color so a given sender always looks the same.
    var avatarColor: Color {
        let palette: [Color] = [
            Color(red: 1.00, green: 0.58, blue: 0.00),
            Color(red: 1.00, green: 0.21, blue: 0.37),
            Color(red: 0.37, green: 0.36, blue: 0.90),
            Color(red: 0.20, green: 0.78, blue: 0.35),
            Color(red: 0.04, green: 0.52, blue: 1.00),
            Color(red: 0.69, green: 0.32, blue: 0.87),
            Color(red: 1.00, green: 0.62, blue: 0.04),
        ]
        let sum = from.unicodeScalars.reduce(0) { $0 + Int($1.value) }
        return palette[sum % palette.count]
    }
}

// MARK: - Gmail API decoding

/// Raw shape of GET /messages/{id}?format=metadata
struct GmailMessageDTO: Decodable {
    let id: String
    let threadId: String
    let labelIds: [String]?
    let snippet: String?
    let internalDate: String?
    let payload: Payload?

    struct Payload: Decodable { let headers: [Header]? }
    struct Header: Decodable { let name: String; let value: String }
}

extension Email {
    init(dto: GmailMessageDTO) {
        let headers = dto.payload?.headers ?? []
        func header(_ name: String) -> String? {
            headers.first { $0.name.caseInsensitiveCompare(name) == .orderedSame }?.value
        }
        let (name, email) = Email.parseFrom(header("From") ?? "")
        let ms = Double(dto.internalDate ?? "") ?? 0

        self.init(
            id: dto.id,
            threadId: dto.threadId,
            from: name,
            fromEmail: email,
            subject: header("Subject")?.decodingHTMLEntities() ?? "(no subject)",
            snippet: (dto.snippet ?? "").decodingHTMLEntities(),
            date: Date(timeIntervalSince1970: ms / 1000),
            isUnread: dto.labelIds?.contains("UNREAD") ?? false
        )
    }

    /// "Jane Doe <jane@x.com>"  ->  ("Jane Doe", "jane@x.com")
    static func parseFrom(_ raw: String) -> (name: String, email: String) {
        if let lt = raw.firstIndex(of: "<"), let gt = raw.firstIndex(of: ">"), lt < gt {
            let email = String(raw[raw.index(after: lt)..<gt])
            var name = String(raw[..<lt])
                .trimmingCharacters(in: .whitespaces)
                .replacingOccurrences(of: "\"", with: "")
            if name.isEmpty { name = email.components(separatedBy: "@").first ?? email }
            return (name, email)
        }
        let trimmed = raw.trimmingCharacters(in: .whitespaces)
        return (trimmed.isEmpty ? "Unknown" : trimmed, trimmed)
    }
}

extension String {
    /// Gmail snippets arrive HTML-escaped (e.g. &#39;). Decode the common ones.
    func decodingHTMLEntities() -> String {
        var s = self
        let map = ["&amp;": "&", "&#39;": "'", "&quot;": "\"", "&lt;": "<", "&gt;": ">", "&nbsp;": " "]
        for (k, v) in map { s = s.replacingOccurrences(of: k, with: v) }
        return s
    }
}
