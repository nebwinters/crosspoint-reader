import Foundation

/// Thin async wrapper over the Gmail REST API.
struct GmailService {
    let auth: GmailAuth
    private let base = "https://gmail.googleapis.com/gmail/v1/users/me"

    // MARK: Reading

    /// Fetch inbox message ids, then their metadata concurrently.
    func listInbox(max: Int = 25) async throws -> [Email] {
        let token = try await auth.validAccessToken()

        var comps = URLComponents(string: "\(base)/messages")!
        comps.queryItems = [
            .init(name: "q", value: "in:inbox"),
            .init(name: "maxResults", value: "\(max)"),
        ]
        let (data, _) = try await get(comps.url!, token: token)

        struct ListResp: Decodable {
            struct M: Decodable { let id: String }
            let messages: [M]?
        }
        let ids = (try JSONDecoder().decode(ListResp.self, from: data).messages ?? []).map { $0.id }

        return try await withThrowingTaskGroup(of: Email?.self) { group in
            for id in ids {
                group.addTask { try await self.message(id: id, token: token) }
            }
            var emails: [Email] = []
            emails.reserveCapacity(ids.count)
            for try await email in group { if let email { emails.append(email) } }
            return emails.sorted { $0.date > $1.date }
        }
    }

    private func message(id: String, token: String) async throws -> Email? {
        var comps = URLComponents(string: "\(base)/messages/\(id)")!
        comps.queryItems = [
            .init(name: "format", value: "metadata"),
            .init(name: "metadataHeaders", value: "From"),
            .init(name: "metadataHeaders", value: "Subject"),
            .init(name: "metadataHeaders", value: "Date"),
        ]
        let (data, _) = try await get(comps.url!, token: token)
        let dto = try JSONDecoder().decode(GmailMessageDTO.self, from: data)
        return Email(dto: dto)
    }

    // MARK: Triage actions

    func archive(_ email: Email) async throws { try await modify(email.id, remove: ["INBOX"]) }
    func markUnread(_ email: Email) async throws { try await modify(email.id, add: ["UNREAD"]) }
    func markRead(_ email: Email) async throws { try await modify(email.id, remove: ["UNREAD"]) }
    /// Gmail's API has no snooze endpoint, so we remove from inbox and track the
    /// return time locally (see SnoozeStore). The app re-files it on next launch.
    func snooze(_ email: Email) async throws { try await modify(email.id, remove: ["INBOX"]) }
    func unarchive(_ email: Email) async throws { try await modify(email.id, add: ["INBOX"]) }
    func addInbox(_ id: String) async throws { try await modify(id, add: ["INBOX"]) }

    private func modify(_ id: String, add: [String] = [], remove: [String] = []) async throws {
        let token = try await auth.validAccessToken()
        var req = URLRequest(url: URL(string: "\(base)/messages/\(id)/modify")!)
        req.httpMethod = "POST"
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = try JSONSerialization.data(withJSONObject: [
            "addLabelIds": add, "removeLabelIds": remove,
        ])
        try await send(req)
    }

    // MARK: Sending

    func sendMail(to: String, subject: String, body: String) async throws {
        let token = try await auth.validAccessToken()
        let from = auth.userEmail ?? "me"
        let mime = """
        From: \(from)\r
        To: \(to)\r
        Subject: \(subject)\r
        MIME-Version: 1.0\r
        Content-Type: text/plain; charset=UTF-8\r
        \r
        \(body)
        """
        var req = URLRequest(url: URL(string: "\(base)/messages/send")!)
        req.httpMethod = "POST"
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = try JSONSerialization.data(withJSONObject: [
            "raw": Data(mime.utf8).base64URLEncoded(),
        ])
        try await send(req)
    }

    // MARK: Plumbing

    private func get(_ url: URL, token: String) async throws -> (Data, URLResponse) {
        var req = URLRequest(url: url)
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        let (data, resp) = try await URLSession.shared.data(for: req)
        try Self.check(resp, data)
        return (data, resp)
    }

    @discardableResult
    private func send(_ req: URLRequest) async throws -> Data {
        let (data, resp) = try await URLSession.shared.data(for: req)
        try Self.check(resp, data)
        return data
    }

    private static func check(_ resp: URLResponse, _ data: Data) throws {
        guard let http = resp as? HTTPURLResponse else { return }
        guard (200..<300).contains(http.statusCode) else {
            let body = String(data: data, encoding: .utf8) ?? ""
            throw AuthError.server("Gmail API \(http.statusCode): \(body)")
        }
    }
}
