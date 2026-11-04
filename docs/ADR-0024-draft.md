# ADR-0024: Ruumble inside a Mumble client (embedded client)

Status: **draft for discussion** (2026-11-03). Written by Kevin with Claude. Nothing here is decided: the project owner decides whether, how and where this is built. If accepted, it amends ADR-0009 and guardrails L1/L2 of the [charter](../internal/charter.md).

> **For the project owner and their AI assistant:** section 9 has a prompt you can paste into Claude Code (or any assistant with access to this repo) to review the draft against the code and to work out your answers to the open decisions in section 7. Every proposal below is a suggestion. The table in section 7 has an empty column for your decision.

## 1. Context

Ruumble gets the best of Mumble without touching it: voice stays in the official client, the building is a web page next to it (ADR-0001, ADR-0009). The price is paid in setup and in workarounds, because neither public interface knows enough on its own:

| Today | Why |
|---|---|
| Two windows: Mumble and a browser | The UI is a web page, the voice is a desktop app |
| Plugin download, install, enable by hand | Mumble has no default-enabled plugins (`src/mumble/Settings.h:96-99`, `PluginManager.cpp:346-361`) |
| Pairing by link or code, identity by plausibility (cert hash + address check) | The browser and the client are different programs (ADR-0004, ADR-0012, P7) |
| Join counts as rejected after 3 s without `channelEntered` | The plugin API does not report a refusal (ADR-0003). The client itself knows it: `MainWindow::msgPermissionDenied`, `Messages.cpp:227-275` |
| Service address in the root channel description | The plugin has no other way to find the service (ADR-0010) |

The idea: ship a **Mumble client with Ruumble built in**, so users get one app with Mumble's audio (Opus, global push-to-talk, echo cancellation, all of the client) and the Ruumble UI as its main view. Official Mumble clients with the plugin keep working as today.

A second, later wish is **screen sharing** (section 8). Mumble has no video or screen sharing at all (checked: nothing in `src/` or `Mumble.proto`). A colleague already runs a Mumble server we could use for a pilot.

## 2. What we found in the two codebases

**Mumble client** (checkout at `v1.6.870-449`, Qt 6, Qt Widgets):

- The main window is `MainWindow` with the user tree `qtvUsers` as central widget (`MainWindow.cpp:469-471`) and docks for log and chat. Docks are placed in `setupView()` (`MainWindow.cpp:1497-1525`). A new central view or dock needs a few lines in `setupGui()`, and no change to `MainWindow.ui`.
- The client holds everything Ruumble needs: `Channel` (parent, `iPosition`, `qhLinks`, `qsDesc`, `uiPermissions` with `Write`/`Enter`, `localUserCanEnter`), `ClientUser` (mute/deaf/suppress/recording/hash, `tsState` for talking), `ChannelListenerManager` for listeners. Changes come as Qt signals (`ClientUser::talkingStateChanged`, `UserModel::userMoved`, `MainWindow::channelStateChanged`, `serverSynchronized`). The plugin manager wiring at `MainWindow.cpp:506-514` is a ready template.
- Commands: `ServerHandler::joinChannel` (`ServerHandler.cpp:985`), `MainWindow::setAudioMute`/`setAudioDeaf` (`MainWindow.cpp:2809, 2854`).
- Qt WebEngine and WebChannel are **not used anywhere** and are not in `dependencies.json`. Adding them is the biggest build change.
- Releases: 1.6.870 (2026-03-01) is the current stable; the 1.5 line still gets patch releases every 2-4 months (1.5.901, .915, .939 in 2026). The only remote is `mumble-voip/mumble`.

**Ruumble:**

- `ruumble_core` (`plugin/CMakeLists.txt:41-47`) is a static library **without any Mumble dependency**: `Core` talks to Mumble only through the abstract `MumbleApi` and `Transport` interfaces (`plugin/src/core.h:32-60`) and gets `OpenUrl`/`IsPaired`/`MarkPaired` callbacks. `net.cpp` (IXWebSocket) is the transport. This is the key fact: a forked client can link the existing core unchanged and implement `MumbleApi` on top of the client's own classes.
- The bridge verifies a plugin by session + cert hash via Ice and optionally the address (`hub.ts:372-388`). A core running inside the client passes this exactly like the plugin does.
- `LiveAdapter` always talks to its own origin (`live.ts:184-186`), pairing ends in an `HttpOnly` cookie (`main.ts:248-251`). A Qt WebEngine view that loads the service URL top-level works with this as is (the CSP `frame-ancestors 'none'` only affects frames).
- The bridge needs Ice for more than the tree: `canEnter`, `Write` for board/care/keys/maintenance (`access.ts`), avatars, cert hash. **The bridge stays**, also in the embedded variant.

## 3. Proposal in one picture

```
 Ruumble client (Mumble fork)                              unchanged
┌──────────────────────────────────────────────┐
│ Mumble client (audio, settings, tray, ...)   │
│   └─ src/mumble/ruumble/                      │
│        ├─ QtMumbleApi : MumbleApi ───────────┼─▶ ruumble_core (from this repo, pinned)
│        │    (ClientUser, Channel, ServerHandler)   └─ net.cpp ──wss /ws/plugin──┐
│        └─ RuumbleView (QWebEngineView) ──────┼──https + /ws/ui (cookie)───────┤
└──────────────────────────────────────────────┘                                ▼
                                                   bridge (service) ──Ice──▶ Mumble server
 Official Mumble client + Ruumble plugin + browser ──────────────────────────────┘ (still supported)
```

The forked client is, from the bridge's point of view, **just a plugin plus a browser on the same machine**. That is why phases 0 and 1 need no change to `bridge/`, `web/` or `protocol/`.

## 4. Phases

Each phase is useful on its own and can be the last one.

| Phase | What | Changes in Ruumble | Changes in the fork |
|---|---|---|---|
| **0: built-in plugin** | The client links `ruumble_core` + `net.cpp` and runs it like the plugin. No install, no enabling. The browser still opens via `OpenUrl`. | Make `ruumble_core` consumable from outside (CMake export or `add_subdirectory` friendly, version define on the core) | `QtMumbleApi`, CMake option `ruumble`, skip a separately installed Ruumble plugin |
| **1: one window** | `RuumbleView` as central widget or dock, Mumble's tree and log stay available (toggle or dock). `OpenUrl(pairUrl)` loads the link in the view, so pairing happens silently on first connect. Persistent `QWebEngineProfile` keeps the cookie. | None required. Optional: a `?embedded` flag in the web UI that hides things that make no sense inside the client (e.g. "download the plugin") | `RuumbleView`, WebEngine in the build, own paired state (not the plugin's `plugin.json`) |
| **2: better signals** | The client reports refusals immediately instead of after 3 s (hook in `msgPermissionDenied`), and optionally the reason (`PermissionDenied` type). | `MumbleApi` gets an optional "move rejected" event, `Core` uses it when present; `result` may get a reason field in `protocol` | Emit a signal in `Messages.cpp` |
| **3: screen sharing** | See section 8. Independent of the fork. | New feature in bridge + web | Only the WebEngine permission handling for `getDisplayMedia` |

Not proposed: an `EmbeddedAdapter` that builds the snapshot from the client's own data via `QWebChannel`. It would remove Ice polling for embedded users, but the bridge still needs Ice for the other users, `Write` permissions and avatars, and the web UI would have to keep two data paths in sync. It is listed under rejected options and can be revisited.

## 5. Keeping Mumble up to date

Goals: follow Mumble **release tags** (not `master`), keep the diff against Mumble small and visible, make an update routine work.

- **Small footprint.** New code lives in `src/mumble/ruumble/`. Touch points in existing Mumble files are limited to: `src/mumble/CMakeLists.txt` (option + sources + WebEngine), `MainWindow.cpp` `setupGui()`/`setupView()` (create and place the view), `Messages.cpp` (phase 2 signal), `PluginManager.cpp` (ignore the external Ruumble plugin), branding/installer. Target: under 100 changed lines outside `src/mumble/ruumble/`. Only `QtMumbleApi` touches Mumble internals, so an internal refactor in Mumble breaks one class, not the UI.
- **Update routine.** The existing `mumble-release-watch.yml` already detects new releases weekly. It is extended to: apply our patches onto the new tag, build Linux and Windows, run the fork's tests, and open either a ready update or an issue listing the conflicting files. `git rerere` remembers solved conflicts.
- **Upstream what is general.** A "move rejected" signal or plugin API getters for parent/position would help every plugin. Each accepted upstream PR shrinks our patch stack. (O16, the texture bug, is already on the list.)
- **Ruumble does not depend on the fork.** The plugin path stays first-class, CI keeps testing it, the fork only pins a Ruumble version.

## 6. Where the code lives (open, see D1)

| Option | How | For | Against |
|---|---|---|---|
| **R1: separate fork repo** (`ruumble-client`) | Branch `ruumble/main` = Mumble tag + a few commits, rebased per release. Pulls `ruumble_core` via `FetchContent` at a Ruumble tag. | Normal git workflow in the fork; Ruumble repo stays as it is | Two repos, version pairing between them, rebases rewrite history |
| **R2: everything in this repo** | New `client/`: `client/MUMBLE_VERSION` (tag), `client/patches/*.patch` (the few touch points), `client/src/` (the `ruumble/` sources). CI clones Mumble at the tag, copies `client/src`, applies the patches, builds. Script `tools/client-patches.sh` for export/import (`git format-patch`/`git am`). | Same pattern as `third_party/mumble/` (ADR-0009): pinned tag, visible diff, one repo, one CI, commits to `main` as usual; core and client change in one commit | Editing patches needs the script; full Mumble build in this repo's CI (long, cache needed) |
| **R3: fork with this repo as submodule** | Like R1, but Ruumble as git submodule | Exact pinning | Submodules are friction for everyone |

Suggestion: **R2**, because it continues what ADR-0009 already does and keeps everything where the project owner works. R1 is the better choice if the client should get its own maintainer or release rhythm.

## 7. Decisions for the project owner

| # | Question | Options | Suggestion | Decision |
|---|---|---|---|---|
| D1 | Do we build an embedded client at all? | yes / not now / only phase 0 | yes, phase 0 + 1 as a pilot | |
| D2 | Where does the code live? | R1 / R2 / R3 (section 6) | R2 | |
| D3 | How far do we go? | phase 0 / 1 / 2 | 1, then decide on 2 after the pilot | |
| D4 | Where does the web UI come from inside the client? | (a) loaded from the bridge URL, (b) bundled in the client | (a): the UI updates with the service, no client release needed, the same origin and cookie as today | |
| D5 | Settings and certificate | (a) own settings dir, import Mumble's certificate once, (b) share `~/.config/Mumble` with the official client | (a): no risk of corrupting official settings across versions; same certificate so cert hash and pairing stay valid | |
| D6 | Name and branding | own name (e.g. "Ruumble Desk"), visible "based on Mumble" | own name, to avoid confusion with the official Mumble project | |
| D7 | Platforms and packaging | Linux (AppImage / Flatpak / .deb), Windows installer, macOS later | Linux AppImage + Windows installer | |
| D8 | ADR-0009 and L1/L2 | amend with a "Current state" note / new guardrail "the plugin path stays supported" | amend; L2 holds for official clients | |
| D9 | Mumble base | latest stable (1.6.x) only / also 1.5.x | latest stable only; the plugin covers older clients | |
| D10 | Screen sharing | not now / spike / plan (section 8) | spike after phase 1, independent of D1 | |

## 8. Screen sharing (outlook, not part of the decision)

- Mumble cannot do it, and its protocol has no place for it. It has to come from Ruumble.
- **It does not need the fork.** Browsers already offer `getDisplayMedia`; inside the client, Qt WebEngine offers the same (screen picker via `QWebEnginePage::desktopMediaRequested` in newer Qt 6, to verify in a spike). So it works for plain browser users and embedded users alike.
- **Media path:** ADR-0022 rejected direct WebRTC between browsers (TURN behind NAT). For screen sharing an **SFU** next to the Mumble server is the usual answer (e.g. LiveKit, mediasoup, Janus; all self-hostable), with a TURN server for difficult networks. That is one more container and a public UDP port range.
- **Bridge role:** decides who may share and watch (room = Mumble channel, `Gate` and `canEnter` as today), hands out short-lived SFU tokens, announces "X is sharing" in the snapshot and in the Mumble log. Media never goes through the bridge.
- **UI idea:** the room shows a "screen" tile; clicking it opens the stream next to the board.
- **Pilot on the colleague's server:** needs Mumble server 1.5 or newer with Ice enabled and a read secret reachable from the Ruumble container, the `ruumble:` line in the root channel description, and for sharing the SFU's ports. Their server stays unchanged otherwise.

## 9. Prompt for reviewing this draft with an AI

Paste this into Claude Code in the Ruumble repo (a Mumble checkout next to it helps for section 2):

```
Read docs/decisions/0024-embedded-client.md, CLAUDE.md, docs/internal/charter.md and
ADRs 0001-0004, 0009, 0010 and 0022. I am the owner of Ruumble and decide on this draft.

1. Check every claim about Ruumble in sections 2-5 against the code and tell me where it
   is wrong or incomplete (file:line).
2. Challenge the proposal: what breaks, what did the draft miss (security of the pairing
   inside the client, two cores connecting for the same user, CI cost, licensing of Qt
   WebEngine, maintenance load)?
3. Go through the decisions D1-D10 with me one by one. For each, give me your view and
   ask for my choice; do not decide for me.
4. With my answers, rewrite the ADR in this repo's style (short MADR, status "accepted"
   for what I decided), add it to docs/decisions/README.md, and list the follow-up work
   as a short plan per phase.
```

## Rejected options

- **Voice in the browser** (WebRTC gateway in front of Mumble, like mumble-web). Loses what Mumble is good at: global push-to-talk with the window in the background, audio processing, low latency.
- **`EmbeddedAdapter` via `QWebChannel` with the client as data source.** Two data paths in the web UI, the bridge still needs Ice. Can be revisited if Ice polling ever becomes a problem.
- **Bundling the plugin with the official client.** Mumble has no default-enabled plugins; the user still has to enable it, so little is gained.
- **Following Mumble `master`.** Too much churn; Ruumble targets releases (ADR-0009).

## Consequences

- One app for users who choose the Ruumble client; no plugin install, pairing happens on first connect.
- We build, sign and ship desktop clients for Linux and Windows. Qt WebEngine adds roughly 100-200 MB to the package and a heavy dependency to the build.
- Every Mumble release means applying the patch stack; with the small footprint and the automated check this should be routine, Qt major updates excepted.
- Two client paths (fork and plugin) must keep working; the core is shared, so most code is tested once.
- The plugin and the built-in core must not both connect for the same user (the bridge answers `already-connected`); the fork ignores the external plugin.
- Licences: Mumble is BSD-3 like Ruumble; Qt WebEngine is LGPL/GPL and contains Chromium; this needs an entry in `THIRD_PARTY_NOTICES.md` for the client.
