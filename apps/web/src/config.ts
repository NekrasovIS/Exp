// Base URLs for the three backend services (issue #264) — mirrors
// DeviceHub's own env-var-driven configuration (AUTH_SERVICE_HOST/PORT
// etc., see README), ported to Vite's `import.meta.env` convention.
// Defaults match docker-compose.yml's ports for local development.

export const authServiceUrl = import.meta.env.VITE_AUTH_SERVICE_URL ?? "http://127.0.0.1:8080";
export const userServiceUrl = import.meta.env.VITE_USER_SERVICE_URL ?? "http://127.0.0.1:8081";
export const chatServiceRestUrl = import.meta.env.VITE_CHAT_SERVICE_REST_URL ?? "http://127.0.0.1:8082";
export const chatServiceWsUrl = import.meta.env.VITE_CHAT_SERVICE_WS_URL ?? "ws://127.0.0.1:8083/";
