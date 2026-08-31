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
    -- Community-scoped role (issue #114) — the owner is tracked
    -- separately on communities.owner_login, never here.
    is_moderator BOOLEAN NOT NULL DEFAULT FALSE,
    PRIMARY KEY (community_id, member_login)
);

-- File attachments (issue #116) — the raw bytes, base64-encoded (as
-- sent by the client — stored verbatim, never re-encoded server-side),
-- directly in this same Postgres database as a TEXT column rather than
-- BYTEA/on-disk/object storage: base64-as-TEXT sidesteps libpqxx's
-- binary-parameter binding entirely, at the cost of ~33% storage
-- overhead — an acceptable simplification given the enforced size cap
-- (kMaxAttachmentSizeBytes in ChatRepository.cpp) and that this isn't
-- meant to scale to large files or real production traffic. A message
-- optionally references one row here (messages.attachment_id below).
CREATE TABLE IF NOT EXISTS attachments (
    id BIGSERIAL PRIMARY KEY,
    channel_id BIGINT NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    uploader_login TEXT NOT NULL,
    filename TEXT NOT NULL,
    content_type TEXT NOT NULL,
    data_base64 TEXT NOT NULL,
    size_bytes BIGINT NOT NULL,
    uploaded_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS messages (
    id BIGSERIAL PRIMARY KEY,
    channel_id BIGINT NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    author_login TEXT NOT NULL,
    body TEXT NOT NULL,
    sent_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- NULL until edited (issue #107) — lets clients show an "(edited)"
    -- marker without a separate history/audit table for a first pass.
    edited_at TIMESTAMPTZ,
    -- NULL for a plain text message (issue #116). ON DELETE SET NULL
    -- rather than CASCADE — an attachment being removed shouldn't take
    -- the message itself down with it (no independent attachment-delete
    -- path exists yet, but this is the safer default if one's added later).
    attachment_id BIGINT REFERENCES attachments(id) ON DELETE SET NULL
);

-- ADD COLUMN IF NOT EXISTS rather than relying solely on the CREATE
-- TABLE above: this script only runs on a container's first startup
-- (postgres docker-entrypoint-initdb.d), so an already-initialized
-- database needs this to actually pick up the new column.
ALTER TABLE messages ADD COLUMN IF NOT EXISTS edited_at TIMESTAMPTZ;
ALTER TABLE messages ADD COLUMN IF NOT EXISTS attachment_id BIGINT REFERENCES attachments(id) ON DELETE SET NULL;
ALTER TABLE memberships ADD COLUMN IF NOT EXISTS is_moderator BOOLEAN NOT NULL DEFAULT FALSE;

CREATE INDEX IF NOT EXISTS messages_channel_id_sent_at_idx ON messages (channel_id, sent_at);
