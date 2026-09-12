// Small shared piece (issue #221): binding a MediaStream to a <video>
// element's srcObject isn't declarative JSX — has to happen in an
// effect via a ref, same as any other imperative DOM API. Kept as its
// own component (CLAUDE.md — utilities/reusable pieces get their own
// file) since both the local preview and every remote tile in
// CallPanel need exactly this.

import { useEffect, useRef } from "react";

import styles from "./VideoTile.module.css";

interface VideoTileProps {
  stream: MediaStream;
  label: string;
  /** Local self-preview must never play its own audio back (feedback) —
   * remote tiles default to false so the peer's voice is actually heard. */
  muted?: boolean;
}

export function VideoTile({ stream, label, muted = false }: VideoTileProps) {
  const videoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    const video = videoRef.current;
    if (video !== null) {
      video.srcObject = stream;
    }
  }, [stream]);

  return (
    <figure className={styles.tile}>
      <video ref={videoRef} autoPlay playsInline muted={muted} />
      <figcaption>{label}</figcaption>
    </figure>
  );
}
