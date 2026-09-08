// Route structure for the web client (issue #264) — #265-#269 fill in
// LoginPage/HomePage's real content; this only owns which URL maps to
// which screen and which screens require a session.

import { BrowserRouter, Route, Routes } from "react-router-dom";

import { HomePage } from "./pages/HomePage.js";
import { LoginPage } from "./pages/LoginPage.js";
import { ProtectedRoute } from "./session/ProtectedRoute.js";
import { SessionProvider } from "./session/SessionContext.js";

export function App() {
  return (
    <SessionProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route element={<ProtectedRoute />}>
            <Route path="/" element={<HomePage />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </SessionProvider>
  );
}
