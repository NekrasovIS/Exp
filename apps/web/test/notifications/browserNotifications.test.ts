import { afterEach, describe, expect, it, vi } from "vitest";

import {
  requestNotificationPermission,
  shouldNotify,
  showMessageNotification,
} from "../../src/notifications/browserNotifications.js";

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("shouldNotify", () => {
  it("is false when the tab isn't hidden, regardless of author", () => {
    expect(shouldNotify(false, "bob", "alice")).toBe(false);
  });

  it("is false for the current user's own message even while hidden", () => {
    expect(shouldNotify(true, "alice", "alice")).toBe(false);
  });

  it("is true for another author's message while hidden", () => {
    expect(shouldNotify(true, "bob", "alice")).toBe(true);
  });

  it("is true when currentLogin is null (no signed-in user known yet)", () => {
    expect(shouldNotify(true, "bob", null)).toBe(true);
  });
});

describe("requestNotificationPermission", () => {
  it("does nothing when the Notification API is unavailable", () => {
    vi.stubGlobal("Notification", undefined);
    expect(() => requestNotificationPermission()).not.toThrow();
  });

  it("requests permission when it's still the default (unasked) state", () => {
    const requestPermission = vi.fn().mockResolvedValue("granted");
    vi.stubGlobal("Notification", { permission: "default", requestPermission });

    requestNotificationPermission();

    expect(requestPermission).toHaveBeenCalledTimes(1);
  });

  it("does not re-prompt once permission was already granted or denied", () => {
    const requestPermission = vi.fn();
    vi.stubGlobal("Notification", { permission: "granted", requestPermission });

    requestNotificationPermission();

    expect(requestPermission).not.toHaveBeenCalled();
  });
});

describe("showMessageNotification", () => {
  it("does nothing when the Notification API is unavailable", () => {
    vi.stubGlobal("Notification", undefined);
    expect(() => showMessageNotification("bob", "hi")).not.toThrow();
  });

  it("does nothing when permission was never granted", () => {
    const NotificationCtor = vi.fn();
    vi.stubGlobal("Notification", Object.assign(NotificationCtor, { permission: "default" }));

    showMessageNotification("bob", "hi");

    expect(NotificationCtor).not.toHaveBeenCalled();
  });

  it("constructs a Notification with the author as title and body as the message", () => {
    const NotificationCtor = vi.fn();
    vi.stubGlobal("Notification", Object.assign(NotificationCtor, { permission: "granted" }));

    showMessageNotification("bob", "hi there");

    expect(NotificationCtor).toHaveBeenCalledWith("bob", { body: "hi there" });
  });
});
