import SwiftUI

/// Bottom compose bar: a "To:" field, an auto-growing body, and a send button.
struct ComposeBar: View {
    /// Returns true on success so the bar can clear itself.
    let onSend: (_ to: String, _ body: String) async -> Bool

    @State private var to = ""
    @State private var messageText = ""
    @State private var sending = false
    @FocusState private var focused: Bool

    private var canSend: Bool {
        !sending && !to.trimmingCharacters(in: .whitespaces).isEmpty
            && !messageText.trimmingCharacters(in: .whitespaces).isEmpty
    }

    var body: some View {
        HStack(alignment: .bottom, spacing: 10) {
            VStack(alignment: .leading, spacing: 4) {
                TextField("To:", text: $to)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .keyboardType(.emailAddress)
                Divider()
                TextField("Write an email…", text: $messageText, axis: .vertical)
                    .font(.body)
                    .lineLimit(1...5)
                    .focused($focused)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 9)
            .background(Color(.secondarySystemGroupedBackground), in: RoundedRectangle(cornerRadius: 20))

            Button {
                Task { await send() }
            } label: {
                Group {
                    if sending { ProgressView().tint(.white) }
                    else { Image(systemName: "arrow.up").fontWeight(.bold) }
                }
                .frame(width: 40, height: 40)
                .background(.tint, in: Circle())
                .foregroundStyle(.white)
            }
            .disabled(!canSend)
            .opacity(canSend ? 1 : 0.4)
        }
        .padding(.horizontal, 12)
        .padding(.top, 10)
        .padding(.bottom, 8)
        .background(.bar)
    }

    private func send() async {
        sending = true
        let ok = await onSend(to, messageText)
        sending = false
        if ok {
            to = ""
            messageText = ""
            focused = false
        }
    }
}
