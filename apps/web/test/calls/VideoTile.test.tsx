import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { VideoTile } from "../../src/calls/VideoTile.js";

function fakeMediaStream(): MediaStream {
  return { getAudioTracks: () => [], getVideoTracks: () => [] } as unknown as MediaStream;
}

describe("VideoTile", () => {
  it("defaults to the camera kind when none is given", () => {
    render(<VideoTile stream={fakeMediaStream()} label="alice" />);

    expect(screen.getByText("alice").closest("figure")).toHaveAttribute("data-kind", "camera");
  });

  it("issue #446 — a screen-share tile is marked with data-kind screen for its own wider/uncropped CSS", () => {
    render(<VideoTile stream={fakeMediaStream()} label="You (screen)" kind="screen" />);

    expect(screen.getByText("You (screen)").closest("figure")).toHaveAttribute("data-kind", "screen");
  });

  it("plays back the given stream and respects the muted prop", () => {
    const stream = fakeMediaStream();
    render(<VideoTile stream={stream} label="bob" muted />);

    const video = screen.getByText("bob").closest("figure")?.querySelector("video");
    expect(video).not.toBeNull();
    expect(video?.srcObject).toBe(stream);
    expect(video).toHaveProperty("muted", true);
  });
});
