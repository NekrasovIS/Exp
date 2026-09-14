import { MessageRow } from "@devicehub/ui";

// MessageRow renders an <li>, so every story needs the same <ul> wrapper
// its two real consumers (apps/web's MessageList/DirectMessageList) always
// provide — a bare <li> outside a list loses its role semantics.

export function OtherMessage() {
  return (
    <ul>
      <MessageRow isOwn={false} author="alice">
        <span>Did you see the new build fail on main?</span>
      </MessageRow>
    </ul>
  );
}

export function OwnMessage() {
  return (
    <ul>
      <MessageRow isOwn={true} author="bob">
        <span>Yeah, looking at it now — think it's the linker.</span>
      </MessageRow>
    </ul>
  );
}

export function Conversation() {
  return (
    <ul>
      <MessageRow isOwn={false} author="alice">
        <span>Did you see the new build fail on main?</span>
      </MessageRow>
      <MessageRow isOwn={true} author="bob">
        <span>Yeah, looking at it now — think it's the linker.</span>
      </MessageRow>
      <MessageRow id="message-42" isOwn={false} author="alice">
        <span>Let me know once it's green again.</span>
      </MessageRow>
    </ul>
  );
}
