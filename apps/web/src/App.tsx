// Route structure for the web client (issue #264) — #265-#269 fill in
// LoginPage/HomePage's real content; this only owns which URL maps to
// which screen and which screens require a session.

import { BrowserRouter, Route, Routes } from "react-router-dom";

import { HomePage } from "./pages/HomePage.js";
import { JoinPage } from "./pages/JoinPage.js";
import { LoginPage } from "./pages/LoginPage.js";
import { ProtectedRoute } from "./session/ProtectedRoute.js";
import { SessionProvider } from "./session/SessionContext.js";

export function App() {
  return (
    <SessionProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          {/* issue #301 — outside ProtectedRoute: JoinPage itself
              decides whether to join now or stash the code and send an
              unauthenticated visitor to /login first. */}
          <Route path="/join/:code" element={<JoinPage />} />
          <Route element={<ProtectedRoute />}>
            <Route path="/" element={<HomePage />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </SessionProvider>
  );
}
