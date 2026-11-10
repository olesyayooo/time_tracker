-- Выполняется на пустой БД. Для существующей БД — вручную, см. README.
BEGIN;
CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(100) NOT NULL CHECK (length(btrim(name)) > 0),
    email VARCHAR(254) NOT NULL,
    password_hash VARCHAR(255) NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS users_email_lower_unique ON users (lower(email));
CREATE TABLE IF NOT EXISTS projects (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    name VARCHAR(150) NOT NULL CHECK (length(btrim(name)) > 0),
    is_completed BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS projects_user_id_idx ON projects(user_id);
CREATE TABLE IF NOT EXISTS tasks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    project_id UUID NOT NULL REFERENCES projects(id) ON DELETE RESTRICT,
    title VARCHAR(200) NOT NULL CHECK (length(btrim(title)) > 0),
    is_completed BOOLEAN NOT NULL DEFAULT FALSE,
    is_paused BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT tasks_completed_not_paused CHECK (NOT (is_completed AND is_paused))
);
CREATE INDEX IF NOT EXISTS tasks_project_id_idx ON tasks(project_id);
-- Рабочие интервалы: пауза закрывает интервал, продолжение создаёт новый.
-- Состояние паузы хранится в tasks.is_paused, не в time_entries.
CREATE EXTENSION IF NOT EXISTS btree_gist;
CREATE TABLE IF NOT EXISTS time_entries (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    task_id UUID NOT NULL REFERENCES tasks(id) ON DELETE RESTRICT,
    started_at TIMESTAMPTZ NOT NULL,
    ended_at TIMESTAMPTZ,
    CONSTRAINT time_entries_positive_duration CHECK (ended_at IS NULL OR ended_at > started_at),
    CONSTRAINT time_entries_no_overlap EXCLUDE USING gist (
        user_id WITH =,
        tstzrange(started_at, ended_at, '[)') WITH &&
    )
);
CREATE UNIQUE INDEX IF NOT EXISTS time_entries_one_active_user
    ON time_entries(user_id) WHERE ended_at IS NULL;
CREATE INDEX IF NOT EXISTS time_entries_task_id_idx ON time_entries(task_id);
COMMIT;
