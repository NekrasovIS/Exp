// React state wrapper around CallManager (issue #221) — mirrors the
// split DeviceHub already has between CallManager (Qt-signal-based,
// framework-agnostic) and MainWindow (owns the actual UI state), just
// with a hook instead of a widget class on the UI side.

import { ChatClient } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";

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
  const manager = useMemo(() => new CallManager(chatClient, localLogin), [chatClient, localLogin]);
  const [state, setState] = useState<CallState>(kInitialState);
  const hideReactionTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
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
      if (hideReactionTimer.current !== null) {
        clearTimeout(hideReactionTimer.current);
      }
    };
  }, [manager]);

  const join = useCallback(() => {
    void manager
      .joinCall()
      .then(() => setState((prev) => ({ ...prev, inCall: true, muted: manager.isMuted() })));
  }, [manager]);

  const leave = useCallback(() => {
    manager.leaveCall();
    setState(kInitialState);
  }, [manager]);

  const toggleMute = useCallback(() => {
    manager.setMuted(!manager.isMuted());
    setState((prev) => ({ ...prev, muted: manager.isMuted() }));
  }, [manager]);

  const toggleVideo = useCallback(() => {
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
  }, [manager]);

  const toggleScreenShare = useCallback(() => {
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
  }, [manager]);

  const sendReaction = useCallback((emoji: string) => manager.sendReaction(emoji), [manager]);

  return [state, { join, leave, toggleMute, toggleVideo, toggleScreenShare, sendReaction }];
}
