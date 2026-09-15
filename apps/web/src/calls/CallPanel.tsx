// Call controls + video tiles for one channel (issue #221) — mirrors
// the set of actions DeviceHub's ChatView/CallWindow expose (join/
// leave, mute, camera, screen-share, participant list), rendered
// inline above the message list rather than in a separate window
// (there's no multi-window desktop-app equivalent to reach for here).

import { ChatClient } from "@devicehub/core";

import styles from "./CallPanel.module.css";
import { usePushToTalk, usePushToTalkPreference } from "./pushToTalk.js";
import { kCallReactionEmojis, useCall } from "./useCall.js";
import { VideoTile } from "./VideoTile.js";
import { reactionEmojiName } from "../reactionEmojiNames.js";

interface CallPanelProps {
  chatClient: ChatClient;
  localLogin: string;
}

export function CallPanel({ chatClient, localLogin }: CallPanelProps) {
  const [state, actions] = useCall(chatClient, localLogin);
  const { enabled: pushToTalkEnabled, setEnabled: setPushToTalkEnabled } = usePushToTalkPreference();
  // Issue #461 — only actually hold-to-talk once both the preference is
  // on AND a call is joined; a stray keydown before/after a call (or
  // while the preference is off) has nothing to do.
  usePushToTalk(pushToTalkEnabled && state.inCall, actions.setMuted);

  if (!state.inCall) {
    return (
      <div className={styles.container}>
        <div className={styles.joinRow}>
          <button type="button" data-variant="primary" onClick={actions.join}>
            Join call
          </button>
          <label className={styles.pushToTalkToggle}>
            <input
              type="checkbox"
              checked={pushToTalkEnabled}
              onChange={(event) => setPushToTalkEnabled(event.target.checked)}
            />
            Push to talk (hold Ctrl)
          </label>
          {state.error !== null && <p role="alert">{state.error}</p>}
        </div>
      </div>
    );
  }

  return (
    <div className={styles.container}>
      <div className={styles.controls}>
        {pushToTalkEnabled ? (
          <span aria-live="polite">{state.muted ? "🔴 Hold Ctrl to talk" : "🟢 Speaking"}</span>
        ) : (
          <button type="button" aria-pressed={state.muted} onClick={actions.toggleMute}>
            {state.muted ? "Unmute" : "Mute"}
          </button>
        )}
        <label className={styles.pushToTalkToggle}>
          <input
            type="checkbox"
            checked={pushToTalkEnabled}
            onChange={(event) => setPushToTalkEnabled(event.target.checked)}
          />
          Push to talk
        </label>
        <button type="button" aria-pressed={state.videoEnabled} onClick={actions.toggleVideo}>
          {state.videoEnabled ? "Disable video" : "Enable video"}
        </button>
        <button type="button" aria-pressed={state.screenShareEnabled} onClick={actions.toggleScreenShare}>
          {state.screenShareEnabled ? "Stop sharing" : "Share screen"}
        </button>
        <button type="button" data-variant="danger" onClick={actions.leave}>
          Leave call
        </button>
      </div>
      {state.error !== null && <p role="alert">{state.error}</p>}
      {state.participants.length > 0 && (
        <p className={styles.participants}>In call: {state.participants.join(", ")}</p>
      )}
      <div className={styles.reactions}>
        {kCallReactionEmojis.map((emoji) => (
          <button
            key={emoji}
            type="button"
            className={styles.reactionButton}
            aria-label={reactionEmojiName(emoji)}
            onClick={() => actions.sendReaction(emoji)}
          >
            {emoji}
          </button>
        ))}
      </div>
      {state.lastReaction !== null && (
        <p className={styles.reactionFeed}>
          {state.lastReaction.login} {state.lastReaction.emoji}
        </p>
      )}
      <div className={styles.videoGrid}>
        {state.localCameraStream !== null && (
          <VideoTile stream={state.localCameraStream} label="You (camera)" muted />
        )}
        {state.localScreenShareStream !== null && (
          <VideoTile stream={state.localScreenShareStream} label="You (screen)" muted kind="screen" />
        )}
        {[...state.remoteStreams.entries()].map(([peerLogin, stream]) => (
          <VideoTile key={peerLogin} stream={stream} label={peerLogin} />
        ))}
      </div>
    </div>
  );
}
