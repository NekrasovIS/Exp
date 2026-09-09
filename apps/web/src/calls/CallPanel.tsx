// Call controls + video tiles for one channel (issue #221) — mirrors
// the set of actions DeviceHub's ChatView/CallWindow expose (join/
// leave, mute, camera, screen-share, participant list), rendered
// inline above the message list rather than in a separate window
// (there's no multi-window desktop-app equivalent to reach for here).

import { ChatClient } from "@devicehub/core";

import { useCall } from "./useCall.js";
import { VideoTile } from "./VideoTile.js";

interface CallPanelProps {
  chatClient: ChatClient;
  localLogin: string;
}

export function CallPanel({ chatClient, localLogin }: CallPanelProps) {
  const [state, actions] = useCall(chatClient, localLogin);

  if (!state.inCall) {
    return (
      <div>
        <button type="button" onClick={actions.join}>
          Join call
        </button>
        {state.error !== null && <p role="alert">{state.error}</p>}
      </div>
    );
  }

  return (
    <div>
      <div>
        <button type="button" onClick={actions.toggleMute}>
          {state.muted ? "Unmute" : "Mute"}
        </button>
        <button type="button" onClick={actions.toggleVideo}>
          {state.videoEnabled ? "Disable video" : "Enable video"}
        </button>
        <button type="button" onClick={actions.toggleScreenShare}>
          {state.screenShareEnabled ? "Stop sharing" : "Share screen"}
        </button>
        <button type="button" onClick={actions.leave}>
          Leave call
        </button>
      </div>
      {state.error !== null && <p role="alert">{state.error}</p>}
      {state.participants.length > 0 && <p>In call: {state.participants.join(", ")}</p>}
      <div>
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
