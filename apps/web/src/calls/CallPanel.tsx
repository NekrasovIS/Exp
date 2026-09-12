// Call controls + video tiles for one channel (issue #221) — mirrors
// the set of actions DeviceHub's ChatView/CallWindow expose (join/
// leave, mute, camera, screen-share, participant list), rendered
// inline above the message list rather than in a separate window
// (there's no multi-window desktop-app equivalent to reach for here).

import { ChatClient } from "@devicehub/core";

import styles from "./CallPanel.module.css";
import { kCallReactionEmojis, useCall } from "./useCall.js";
import { VideoTile } from "./VideoTile.js";

interface CallPanelProps {
  chatClient: ChatClient;
  localLogin: string;
}

export function CallPanel({ chatClient, localLogin }: CallPanelProps) {
  const [state, actions] = useCall(chatClient, localLogin);

  if (!state.inCall) {
    return (
      <div className={styles.container}>
        <div className={styles.joinRow}>
          <button type="button" data-variant="primary" onClick={actions.join}>
            Join call
          </button>
          {state.error !== null && <p role="alert">{state.error}</p>}
        </div>
      </div>
    );
  }

  return (
    <div className={styles.container}>
      <div className={styles.controls}>
        <button type="button" aria-pressed={state.muted} onClick={actions.toggleMute}>
          {state.muted ? "Unmute" : "Mute"}
        </button>
        <button type="button" aria-pressed={state.videoEnabled} onClick={actions.toggleVideo}>
          {state.videoEnabled ? "Disable video" : "Enable video"}
        </button>
        <button type="button" aria-pressed={state.screenShareEnabled} onClick={actions.toggleScreenShare}>
          {state.screenShareEnabled ? "Stop sharing" : "Share screen"}
        </button>
        <button type="button" className={styles.leaveButton} onClick={actions.leave}>
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
          <VideoTile stream={state.localScreenShareStream} label="You (screen)" muted />
        )}
        {[...state.remoteStreams.entries()].map(([peerLogin, stream]) => (
          <VideoTile key={peerLogin} stream={stream} label={peerLogin} />
        ))}
      </div>
    </div>
  );
}
