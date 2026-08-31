CREATE TABLE IF NOT EXISTS communities (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    owner_login TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS channels (
    id BIGSERIAL PRIMARY KEY,
    community_id BIGINT NOT NULL REFERENCES communities(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    owner_login TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (community_id, name)
);

-- member_login/author_login reference user-service's users by login, not
-- a foreign key: chat-service owns its own database (database-per-service)
-- and never reaches into user-service's Postgres directly. Callers are
-- authenticated via auth-service tokens before any of these rows are
-- written — see AuthServiceClient.
CREATE TABLE IF NOT EXISTS memberships (
    community_id BIGINT NOT NULL REFERENCES communities(id) ON DELETE CASCADE,
    member_login TEXT NOT NULL,
    joined_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (community_id, member_login)
);

CREATE TABLE IF NOT EXISTS messages (
    id BIGSERIAL PRIMARY KEY,
    channel_id BIGINT NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    author_login TEXT NOT NULL,
    body TEXT NOT NULL,
    sent_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- NULL until edited (issue #107) — lets clients show an "(edited)"
    -- marker without a separate history/audit table for a first pass.
    edited_at TIMESTAMPTZ
);

-- ADD COLUMN IF NOT EXISTS rather than relying solely on the CREATE
-- TABLE above: this script only runs on a container's first startup
-- (postgres docker-entrypoint-initdb.d), so an already-initialized
-- database needs this to actually pick up the new column.
ALTER TABLE messages ADD COLUMN IF NOT EXISTS edited_at TIMESTAMPTZ;

CREATE INDEX IF NOT EXISTS messages_channel_id_sent_at_idx ON messages (channel_id, sent_at);
