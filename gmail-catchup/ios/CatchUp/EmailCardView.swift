import SwiftUI

/// A swipeable inbox row:
///   swipe left          → mark unread
///   hold (~0.28s) + left → snooze
///   swipe right         → archive
struct EmailCardView: View {
    let email: Email
    let onAction: (ActionKind) -> Void

    @State private var offset: CGFloat = 0
    @State private var armed = false        // snooze armed via press-and-hold
    @State private var committed = false

    private let threshold: CGFloat = 96
    private let flyAway: CGFloat = 600

    /// Action implied by the current drag direction.
    private var pendingAction: ActionKind {
        if offset > 0 { return .archive }
        return armed ? .snooze : .unread
    }

    var body: some View {
        ZStack {
            revealBackground
            card
                .offset(x: offset)
                .gesture(combinedGesture)
        }
        .animation(.spring(response: 0.3, dampingFraction: 0.82), value: offset)
        .sensoryFeedback(.impact(weight: .medium), trigger: armed) { _, now in now }
        .sensoryFeedback(.success, trigger: committed) { _, now in now }
    }

    // MARK: Background revealed under the card

    private var revealBackground: some View {
        let action = pendingAction
        let progress = min(1, abs(offset) / threshold)
        return RoundedRectangle(cornerRadius: 18)
            .fill(action.tint)
            .overlay(alignment: offset > 0 ? .leading : .trailing) {
                Label(actionTitle(action), systemImage: action.systemImage)
                    .font(.subheadline.weight(.bold))
                    .foregroundStyle(.white)
                    .labelStyle(IconLeadingIfArchive(isLeading: offset > 0))
                    .padding(.horizontal, 24)
                    .opacity(progress)
            }
    }

    private func actionTitle(_ a: ActionKind) -> String {
        switch a {
        case .archive: return "Archive"
        case .unread: return "Unread"
        case .snooze: return "Snooze"
        }
    }

    // MARK: Card

    private var card: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle().fill(email.avatarColor)
                Text(email.initials).font(.headline).foregroundStyle(.white)
            }
            .frame(width: 44, height: 44)

            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    if email.isUnread {
                        Circle().fill(Color(red: 0.04, green: 0.52, blue: 1.0)).frame(width: 8, height: 8)
                    }
                    Text(email.from)
                        .font(.system(size: 15, weight: email.isUnread ? .bold : .semibold))
                        .lineLimit(1)
                    Spacer(minLength: 4)
                    Text(email.timeLabel).font(.caption).foregroundStyle(.secondary)
                }
                Text(email.subject)
                    .font(.system(size: 14, weight: email.isUnread ? .semibold : .regular))
                    .lineLimit(1)
                Text(email.snippet)
                    .font(.system(size: 13))
                    .foregroundStyle(.secondary)
                    .lineLimit(2)
            }
        }
        .padding(14)
        .background(Color(.secondarySystemGroupedBackground), in: RoundedRectangle(cornerRadius: 18))
        .overlay {
            if armed {
                RoundedRectangle(cornerRadius: 18)
                    .strokeBorder(ActionKind.snooze.tint, lineWidth: 2)
            }
        }
    }

    // MARK: Gesture

    private var combinedGesture: some Gesture {
        let drag = DragGesture(minimumDistance: 8)
            .onChanged { value in
                guard !committed else { return }
                // Let vertical scrolling win.
                if abs(value.translation.height) > abs(value.translation.width) + 4 { return }
                offset = value.translation.width
            }
            .onEnded { _ in
                guard !committed else { return }
                if abs(offset) > threshold {
                    commit(pendingAction)
                } else {
                    offset = 0
                    armed = false
                }
            }

        // Fires only if the finger is held still (~0.28s) before swiping → arms snooze.
        let hold = LongPressGesture(minimumDuration: 0.28, maximumDistance: 30)
            .onEnded { _ in if !committed { armed = true } }

        return hold.simultaneously(with: drag)
    }

    private func commit(_ kind: ActionKind) {
        committed = true
        offset = (kind == .archive ? 1 : -1) * flyAway
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.22) { onAction(kind) }
    }
}

/// Keeps the icon on the inside edge for archive (revealed from the left).
private struct IconLeadingIfArchive: LabelStyle {
    let isLeading: Bool
    func makeBody(configuration: Configuration) -> some View {
        HStack(spacing: 8) {
            if isLeading { configuration.icon; configuration.title }
            else { configuration.title; configuration.icon }
        }
    }
}
