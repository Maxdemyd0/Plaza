CREATE TABLE IF NOT EXISTS apps (
                                    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,

                                    component_id TEXT NOT NULL UNIQUE,
                                    name TEXT NOT NULL,
                                    summary TEXT NOT NULL DEFAULT '',
                                    description TEXT NOT NULL DEFAULT '',
                                    icon_name TEXT NOT NULL DEFAULT 'application-x-executable',
                                    homepage_url TEXT,

                                    is_verified BOOLEAN NOT NULL DEFAULT FALSE,
                                    is_published BOOLEAN NOT NULL DEFAULT FALSE,

                                    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
                                    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS app_sources (
                                           id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,

                                           app_id BIGINT NOT NULL
                                           REFERENCES apps(id)
    ON DELETE CASCADE,

    backend TEXT NOT NULL
    CHECK (
              backend IN (
              'flatpak',
              'dnf',
              'apt',
              'rpm-ostree'
                         )
    ),

    package_id TEXT NOT NULL,
    repository TEXT,
    architecture TEXT,

    priority SMALLINT NOT NULL DEFAULT 100,
    is_available BOOLEAN NOT NULL DEFAULT TRUE,

    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (app_id, backend, package_id)
    );

CREATE INDEX IF NOT EXISTS idx_apps_published_name
    ON apps (is_published, name);

CREATE INDEX IF NOT EXISTS idx_app_sources_app
    ON app_sources (app_id, priority);
