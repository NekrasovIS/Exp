CREATE TABLE IF NOT EXISTS users (
    id BIGSERIAL PRIMARY KEY,
    login TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- Both NULL until the user sets them (issue #110) — display_name
    -- falls back to login, avatar_url to an initial-letter icon, on the
    -- client side rather than defaulting them here.
    display_name TEXT,
    avatar_url TEXT,
    -- Base64-encoded X25519 public key (issue #136, E2E encryption
    -- Phase 1) — the private half never reaches this server; NULL until
    -- the client generates a keypair and publishes the public half.
    public_key TEXT
);

-- ADD COLUMN IF NOT EXISTS rather than relying solely on the CREATE
-- TABLE above: this script only runs on a container's first startup
-- (postgres docker-entrypoint-initdb.d), so an already-initialized
-- database needs this to actually pick up the new columns.
ALTER TABLE users ADD COLUMN IF NOT EXISTS display_name TEXT;
ALTER TABLE users ADD COLUMN IF NOT EXISTS avatar_url TEXT;
ALTER TABLE users ADD COLUMN IF NOT EXISTS public_key TEXT;
