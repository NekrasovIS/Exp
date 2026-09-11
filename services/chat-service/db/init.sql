CREATE TABLE IF NOT EXISTS communities (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    owner_login TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- Приглашения (issue #186) — генерируется на стороне приложения
    -- (ChatRepository::generateInviteCode(), RAND_bytes) при создании
    -- сообщества, а не здесь в SQL; NULL допускается только для строк,
    -- существовавших до этой миграции (ALTER TABLE ниже) — новый код
    -- для них выдаёт POST /communities/{id}/invite/regenerate.
    invite_code TEXT
);

CREATE TABLE IF NOT EXISTS channels (
    id BIGSERIAL PRIMARY KEY,
    community_id BIGINT NOT NULL REFERENCES communities(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    owner_login TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- E2E encryption Phase 2 (issue #138) — set only at creation, never
    -- changed afterwards; see Channel::isEncrypted's doc comment.
    is_encrypted BOOLEAN NOT NULL DEFAULT FALSE,
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

-- One row per (channel, member) holding that member's copy of the
-- channel's symmetric key, sealed with their own X25519 public key
-- (libsodium crypto_box_seal, issue #138) — chat-service never sees the
-- raw key, only these per-recipient wrapped copies. No FK to a "users"
-- table for the same reason member_login/author_login above have none.
CREATE TABLE IF NOT EXISTS channel_keys (
    channel_id BIGINT NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    member_login TEXT NOT NULL,
    wrapped_key TEXT NOT NULL,
    PRIMARY KEY (channel_id, member_login)
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
    attachment_id BIGINT REFERENCES attachments(id) ON DELETE SET NULL,
    -- Reply/quote (issue #306) — deliberately NOT a foreign key: unlike
    -- attachment_id above (where the referenced row disappearing should
    -- detach cleanly via ON DELETE SET NULL), a deleted quoted message
    -- should neither block the delete nor silently erase this reference
    -- — the client resolves the id against its own already-loaded
    -- history and shows "message unavailable" if it can't find it
    -- (already-deleted or just not loaded), same as it would for an id
    -- pointing outside the currently-fetched page.
    reply_to_message_id BIGINT
);

-- ADD COLUMN IF NOT EXISTS rather than relying solely on the CREATE
-- TABLE above: this script only runs on a container's first startup
-- (postgres docker-entrypoint-initdb.d), so an already-initialized
-- database needs this to actually pick up the new column.
ALTER TABLE messages ADD COLUMN IF NOT EXISTS edited_at TIMESTAMPTZ;
ALTER TABLE messages ADD COLUMN IF NOT EXISTS attachment_id BIGINT REFERENCES attachments(id) ON DELETE SET NULL;
ALTER TABLE memberships ADD COLUMN IF NOT EXISTS is_moderator BOOLEAN NOT NULL DEFAULT FALSE;
ALTER TABLE channels ADD COLUMN IF NOT EXISTS is_encrypted BOOLEAN NOT NULL DEFAULT FALSE;
ALTER TABLE communities ADD COLUMN IF NOT EXISTS invite_code TEXT;
ALTER TABLE messages ADD COLUMN IF NOT EXISTS reply_to_message_id BIGINT;

CREATE INDEX IF NOT EXISTS messages_channel_id_sent_at_idx ON messages (channel_id, sent_at);

-- Частичный индекс (не обычный UNIQUE) — допускает сколько угодно строк
-- с invite_code IS NULL (сообщества, созданные до миграции и ещё не
-- получившие код через regenerate), но не позволяет двум ненулевым
-- кодам совпасть.
CREATE UNIQUE INDEX IF NOT EXISTS communities_invite_code_idx ON communities (invite_code) WHERE invite_code IS NOT NULL;

-- Личные диалоги (issue #187, Фаза 2) — переиспользует message-модель,
-- но без community/channel: ровно одна строка на неупорядоченную пару
-- участников, login'ы в каноническом порядке (меньший первым), как и
-- friendships в user-service. Открыть новый диалог можно только между
-- друзьями (проверяется через внутренний GET /internal/friendship на
-- user-service, см. UserServiceClient) — не хранится здесь, поскольку
-- дружба не принадлежит chat-service; уже открытый диалог продолжает
-- работать, даже если дружба позже будет разорвана.
CREATE TABLE IF NOT EXISTS direct_message_threads (
    id BIGSERIAL PRIMARY KEY,
    user_a_login TEXT NOT NULL,
    user_b_login TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CHECK (user_a_login <> user_b_login),
    CHECK (user_a_login < user_b_login),
    UNIQUE (user_a_login, user_b_login)
);

-- Без вложений/редактирования/удаления в этой фазе (в отличие от
-- messages выше) — то же MVP-first сужение, что и у Фазы 1 (без UI).
CREATE TABLE IF NOT EXISTS direct_messages (
    id BIGSERIAL PRIMARY KEY,
    thread_id BIGINT NOT NULL REFERENCES direct_message_threads(id) ON DELETE CASCADE,
    author_login TEXT NOT NULL,
    body TEXT NOT NULL,
    sent_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS direct_messages_thread_id_sent_at_idx ON direct_messages (thread_id, sent_at);

-- SFU-инфраструктура для групповых звонков (issue #123/#231) — сопоставление
-- канала с videoroom-комнатой в Janus (services/janus/). janus_room_id
-- сейчас всегда вычисляется детерминированно ("channel-<id>",
-- ChatService::ensureCallRoom()), поэтому эта таблица не источник истины
-- для самого id, а учёт/идемпотентность создания: JanusClient проверяет
-- существование комнаты в Janus напрямую при каждом call_join (Janus не
-- сохраняет динамически созданные комнаты между перезапусками), а не
-- полагается только на наличие этой строки.
CREATE TABLE IF NOT EXISTS channel_janus_rooms (
    channel_id BIGINT PRIMARY KEY REFERENCES channels(id) ON DELETE CASCADE,
    janus_room_id TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
