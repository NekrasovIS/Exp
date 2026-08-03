CREATE TABLE IF NOT EXISTS communities (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS channels (
    id BIGSERIAL PRIMARY KEY,
    community_id BIGINT NOT NULL REFERENCES communities(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
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
    sent_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS messages_channel_id_sent_at_idx ON messages (channel_id, sent_at);
