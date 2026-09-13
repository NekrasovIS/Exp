import { fireEvent, render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { ProfilePage } from "../../src/profile/ProfilePage.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function seedSession(): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
  );
}

function renderPage() {
  return render(
    <MemoryRouter>
      <SessionProvider>
        <ProfilePage />
      </SessionProvider>
    </MemoryRouter>,
  );
}

beforeEach(() => {
  seedSession();
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("ProfilePage", () => {
  it("loads and prefills the display name field from the current profile", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, {
          login: "alice",
          display_name: "Alice",
          avatar_url: null,
          public_key: null,
          email: null,
          telegram_chat_id: null,
        }),
      ),
    );

    renderPage();

    expect(await screen.findByLabelText("Display name")).toHaveValue("Alice");
  });

  it("saving a new display name sends it to the server", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [
          /\/users\/alice\/profile$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: "Alice",
              avatar_url: null,
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
        [
          /\/users\/me$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: "Alice B.",
              avatar_url: null,
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
      ]),
    );

    renderPage();
    const input = await screen.findByLabelText("Display name");
    await userEvent.clear(input);
    await userEvent.type(input, "Alice B.");
    await userEvent.click(screen.getByRole("button", { name: "Save" }));

    expect(await screen.findByLabelText("Display name")).toHaveValue("Alice B.");
  });

  it("uploading a new avatar file posts it and shows the resulting error on failure", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [
          /\/users\/alice\/profile$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: null,
              avatar_url: null,
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
        [
          /\/profile\/avatar$/,
          () => jsonResponse(400, { error: "'content_type' must be an image/* MIME type" }),
        ],
      ]),
    );

    renderPage();
    await screen.findByLabelText("Display name");

    // fireEvent, not userEvent.upload() — for reasons not fully pinned
    // down, userEvent.upload() never delivers the resulting change
    // event to this particular input in jsdom (MessageComposer.test.tsx
    // uses userEvent.upload() successfully for its own file input, so
    // this isn't a general incompatibility); fireEvent.change() with an
    // explicit FileList exercises the same handleAvatarSelected() path.
    const file = new File(["fake-bytes"], "not-an-image.txt", { type: "text/plain" });
    fireEvent.change(screen.getByLabelText("Change avatar"), { target: { files: [file] } });

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't upload/i);
  });
});
