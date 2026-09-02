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
    public_key TEXT,
    -- NULL, пока пользователь не задаст (issue #156, вход по
    -- одноразовому коду) — нужен, прежде чем можно будет входить по
    -- коду через email, но не при регистрации, чтобы не ломать
    -- существующие аккаунты.
    email TEXT,
    -- NULL, пока пользователь не привяжет свой Telegram (issue #174 —
    -- альтернативный канал доставки OTP-кода): численный chat_id чата
    -- пользователя с ботом DeviceHub, не username и не номер телефона.
    telegram_chat_id TEXT
);

-- ADD COLUMN IF NOT EXISTS rather than relying solely on the CREATE
-- TABLE above: this script only runs on a container's first startup
-- (postgres docker-entrypoint-initdb.d), so an already-initialized
-- database needs this to actually pick up the new columns.
ALTER TABLE users ADD COLUMN IF NOT EXISTS display_name TEXT;
ALTER TABLE users ADD COLUMN IF NOT EXISTS avatar_url TEXT;
ALTER TABLE users ADD COLUMN IF NOT EXISTS public_key TEXT;
ALTER TABLE users ADD COLUMN IF NOT EXISTS email TEXT;
ALTER TABLE users ADD COLUMN IF NOT EXISTS telegram_chat_id TEXT;

-- Частичный уникальный индекс (а не обычное ограничение UNIQUE на
-- колонку), потому что у нескольких пользователей email может быть ещё
-- не задан (NULL) — обычный UNIQUE в Postgres и так трактует NULL как
-- различные значения по этой же причине, но частичный индекс выражает
-- это намерение явно, а не полагается на побочный эффект.
CREATE UNIQUE INDEX IF NOT EXISTS users_email_unique ON users (email) WHERE email IS NOT NULL;
CREATE UNIQUE INDEX IF NOT EXISTS users_telegram_chat_id_unique ON users (telegram_chat_id)
    WHERE telegram_chat_id IS NOT NULL;
