// Guards every route that needs a signed-in session (issue #264) — the
// screens themselves (#265-#269) don't each re-check isAuthenticated.

import { Navigate, Outlet } from "react-router-dom";

import { useSession } from "./SessionContext.js";

export function ProtectedRoute() {
  const { isAuthenticated } = useSession();
  if (!isAuthenticated) {
    return <Navigate to="/login" replace />;
  }
  return <Outlet />;
}
