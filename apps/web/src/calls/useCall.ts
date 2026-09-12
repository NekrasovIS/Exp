// React state wrapper around CallManager (issue #221) — mirrors the
// split DeviceHub already has between CallManager (Qt-signal-based,
// framework-agnostic) and MainWindow (owns the actual UI state), just
// with a hook instead of a widget class on the UI side.

import { ChatClient } from "@devicehub/core";
import { useCallback, useEffect, useRef, useState } from "react";

import { CallManager } from "./CallManager.js";

/** The fixed emoji set for quick in-call reactions (issue #312/#328) —
 * same list as DeviceHub's own CallWindow::reactionEmojis(), so a user
 * sees identical choices on either client. */
export const kCallReactionEmojis = ["👍", "❤️", "😂", "🎉", "👏"];

/** How long a received reaction stays visible before auto-hiding
 * (issue #312/#328) — matches DeviceHub's CallWindow
 * kReactionFeedHideMs. */
const kReactionFeedHideMs = 2500;

export interface ReceivedReaction {
  login: string;
  emoji: string;
}

export interface CallState {
  inCall: boolean;
  muted: boolean;
  videoEnabled: boolean;
  screenShareEnabled: boolean;
  participants: string[];
  localCameraStream: MediaStream | null;
  localScreenShareStream: MediaStream | null;
  remoteStreams: ReadonlyMap<string, MediaStream>;
  /** Most recently received reaction, cleared automatically after
   * kReactionFeedHideMs — null between reactions/once hidden. */
  lastReaction: ReceivedReaction | null;
  error: string | null;
}

export interface CallActions {
  join: () => void;
  leave: () => void;
  toggleMute: () => void;
  toggleVideo: () => void;
  toggleScreenShare: () => void;
  sendReaction: (emoji: string) => void;
}

const kInitialState: CallState = {
  inCall: false,
  muted: false,
  videoEnabled: false,
  screenShareEnabled: false,
  participants: [],
  localCameraStream: null,
  localScreenShareStream: null,
  remoteStreams: new Map(),
  lastReaction: null,
  error: null,
};

/** @p chatClient must already be the same instance subscribed to the
 * channel this call belongs to (see useMessages()'s own doc comment on
 * why the call rides that exact connection) and @p localLogin the
 * current user's own login (never shown as a participant/tile of its
 * own — mirrors CallManager's ownFeedId/localLogin filtering). */
export function useCall(chatClient: ChatClient, localLogin: string): [CallState, CallActions] {
  // Issue #365 — a fresh CallManager per effect run (not useMemo's
  // "stable across renders" instance) so its lifecycle exactly matches
  // the effect's own mount/cleanup, including React StrictMode's
  // deliberate dev-only mount -> cleanup -> mount cycle: CallManager
  // subscribes to chatClient ONCE, in its constructor, and dispose()
  // permanently tears that down — a useMemo'd instance survives
  // StrictMode's synthetic remount (same object, no new render), so its
  // cleanup-triggered dispose() during the discarded first mount left
  // it deaf to every future call_roster/call_peer_joined/janus_event
  // from chatClient for the rest of the component's life, with no way
  // to resubscribe short of a whole new instance. Symptom: nobody ever
  // showed up in the participant list, and remote camera/screen-share
  // never arrived — not a race in the roster fix (issue #362) itself,
  // but this made it and the pre-existing callPeerJoined path both look
  // broken identically, since both ride the same doomed subscription.
  const managerRef = useRef<CallManager | null>(null);
  const [state, setState] = useState<CallState>(kInitialState);
  const hideReactionTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    const manager = new CallManager(chatClient, localLogin);
    managerRef.current = manager;
    const unsubscribes = [
      manager.on("reactionReceived", (login, emoji) => {
        if (hideReactionTimer.current !== null) {
          clearTimeout(hideReactionTimer.current);
        }
        setState((prev) => ({ ...prev, lastReaction: { login, emoji } }));
        hideReactionTimer.current = setTimeout(() => {
          setState((prev) => ({ ...prev, lastReaction: null }));
        }, kReactionFeedHideMs);
      }),
      manager.on("participantJoined", (login) =>
        setState((prev) =>
          prev.participants.includes(login) ? prev : { ...prev, participants: [...prev.participants, login] },
        ),
      ),
      manager.on("participantLeft", (login) =>
        setState((prev) => ({ ...prev, participants: prev.participants.filter((p) => p !== login) })),
      ),
      manager.on("remoteStream", (peerLogin, stream) =>
        setState((prev) => {
          const remoteStreams = new Map(prev.remoteStreams);
          remoteStreams.set(peerLogin, stream);
          return { ...prev, remoteStreams };
        }),
      ),
      manager.on("remoteStreamRemoved", (peerLogin) =>
        setState((prev) => {
          const remoteStreams = new Map(prev.remoteStreams);
          remoteStreams.delete(peerLogin);
          return { ...prev, remoteStreams };
        }),
      ),
      manager.on("localCameraStream", (stream) =>
        setState((prev) => ({ ...prev, localCameraStream: stream })),
      ),
      manager.on("localScreenShareStream", (stream) =>
        setState((prev) => ({ ...prev, localScreenShareStream: stream })),
      ),
      manager.on("error", (message) => setState((prev) => ({ ...prev, error: message }))),
    ];
    return () => {
      unsubscribes.forEach((off) => off());
      manager.dispose();
      managerRef.current = null;
      if (hideReactionTimer.current !== null) {
        clearTimeout(hideReactionTimer.current);
      }
    };
  }, [chatClient, localLogin]);

  const join = useCallback(() => {
    void managerRef.current?.joinCall().then(() => {
      setState((prev) => ({ ...prev, inCall: true, muted: managerRef.current?.isMuted() ?? false }));
    });
  }, []);

  const leave = useCallback(() => {
    managerRef.current?.leaveCall();
    setState(kInitialState);
  }, []);

  const toggleMute = useCallback(() => {
    const manager = managerRef.current;
    if (manager === null) {
      return;
    }
    manager.setMuted(!manager.isMuted());
    setState((prev) => ({ ...prev, muted: manager.isMuted() }));
  }, []);

  const toggleVideo = useCallback(() => {
    const manager = managerRef.current;
    if (manager === null) {
      return;
    }
    if (manager.videoEnabled()) {
      manager.disableVideo();
      setState((prev) => ({ ...prev, videoEnabled: false }));
    } else {
      void manager
        .enableVideo()
        .then(() =>
          setState((prev) => ({ ...prev, videoEnabled: manager.videoEnabled(), screenShareEnabled: false })),
        );
    }
  }, []);

  const toggleScreenShare = useCallback(() => {
    const manager = managerRef.current;
    if (manager === null) {
      return;
    }
    if (manager.screenShareEnabled()) {
      manager.disableScreenShare();
      setState((prev) => ({ ...prev, screenShareEnabled: false }));
    } else {
      void manager.enableScreenShare().then(() =>
        setState((prev) => ({
          ...prev,
          screenShareEnabled: manager.screenShareEnabled(),
          videoEnabled: false,
        })),
      );
    }
  }, []);

  const sendReaction = useCallback((emoji: string) => managerRef.current?.sendReaction(emoji), []);

  return [state, { join, leave, toggleMute, toggleVideo, toggleScreenShare, sendReaction }];
}
