PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS apps (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    component_id TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    summary TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    icon_name TEXT NOT NULL DEFAULT 'application-x-executable',
    homepage_url TEXT,
    is_verified INTEGER NOT NULL DEFAULT 0,
    is_published INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS app_sources (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_id INTEGER NOT NULL REFERENCES apps(id) ON DELETE CASCADE,
    backend TEXT NOT NULL CHECK (backend IN ('flatpak', 'dnf', 'apt', 'rpm-ostree')),
    package_id TEXT NOT NULL,
    repository TEXT,
    architecture TEXT,
    priority INTEGER NOT NULL DEFAULT 100,
    is_available INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (app_id, backend, package_id)
);

CREATE INDEX IF NOT EXISTS idx_apps_published_name ON apps (is_published, name);
CREATE INDEX IF NOT EXISTS idx_app_sources_app ON app_sources (app_id, priority);

INSERT INTO apps (component_id, name, summary, description, icon_name, homepage_url, is_verified, is_published)
VALUES
    ('org.mozilla.firefox', 'Firefox', 'Fast, private web browsing', 'Browse the web with Mozilla Firefox.', 'firefox', 'https://www.mozilla.org/firefox/', 1, 1),
    ('org.kde.krita', 'Krita', 'Digital painting and illustration', 'A professional painting program created by the KDE community.', 'krita', 'https://krita.org', 1, 1),
    ('com.valvesoftware.Steam', 'Steam', 'Games and a worldwide gaming community', 'Install and manage PC games with Steam.', 'steam', 'https://store.steampowered.com', 1, 1),
    ('org.libreoffice.LibreOffice', 'LibreOffice', 'A powerful office suite', 'Create documents, spreadsheets, presentations, and more.', 'libreoffice-startcenter', 'https://www.libreoffice.org/', 1, 1),
    ('org.videolan.VLC', 'VLC', 'Play almost any media format', 'A free and open-source multimedia player.', 'vlc', 'https://www.videolan.org/vlc/', 1, 1),
    ('org.gimp.GIMP', 'GIMP', 'Image editing and digital art', 'A cross-platform image editor for photographs and artwork.', 'gimp', 'https://www.gimp.org/', 1, 1),
    ('com.obsproject.Studio', 'OBS Studio', 'Live streaming and screen recording', 'Capture, mix, and stream video and audio.', 'com.obsproject.Studio', 'https://obsproject.com/', 1, 1),
    ('org.blender.Blender', 'Blender', '3D creation suite', 'Create 3D graphics, animation, visual effects, and more.', 'blender', 'https://www.blender.org/', 1, 1),
    ('com.discordapp.Discord', 'Discord', 'Voice, video, and text chat', 'Talk and hang out with communities and friends.', 'discord', 'https://discord.com/', 1, 1),
    ('com.spotify.Client', 'Spotify', 'Music for every moment', 'Listen to music, podcasts, and playlists.', 'spotify-client', 'https://www.spotify.com/', 1, 1),
    ('com.heroicgameslauncher.hgl', 'Heroic Games Launcher', 'A launcher for Epic, GOG, and Amazon games', 'Install and play games from supported PC game stores.', 'heroic', 'https://heroicgameslauncher.com/', 1, 1),
    ('com.usebottles.bottles', 'Bottles', 'Run Windows software on Linux', 'Manage environments for running Windows applications and games.', 'com.usebottles.bottles', 'https://usebottles.com/', 1, 1),
    ('com.visualstudio.code', 'Visual Studio Code', 'Code editing, redefined', 'A source-code editor with debugging and extension support.', 'com.visualstudio.code', 'https://code.visualstudio.com/', 1, 1),
    ('org.gnome.Calculator', 'Calculator', 'A simple and capable calculator', 'Perform basic and advanced calculations.', 'org.gnome.Calculator', 'https://apps.gnome.org/Calculator/', 1, 1),
    ('org.kde.kdenlive', 'Kdenlive', 'Video editing for everyone', 'Edit video with a multi-track, non-linear editor.', 'kdenlive', 'https://kdenlive.org/', 1, 1),
    ('com.jetbrains.CLion', 'CLion', 'C and C++ development environment', 'A cross-platform C and C++ IDE from JetBrains.', 'clion', 'https://www.jetbrains.com/clion/', 1, 1),
    ('com.jetbrains.Toolbox', 'JetBrains Toolbox', 'Manage your JetBrains development tools', 'Install, update, and manage JetBrains IDEs and developer tools.', 'applications-development', 'https://www.jetbrains.com/toolbox-app/', 1, 1)
ON CONFLICT (component_id) DO UPDATE SET
    name = excluded.name,
    summary = excluded.summary,
    description = excluded.description,
    icon_name = excluded.icon_name,
    homepage_url = excluded.homepage_url,
    is_verified = excluded.is_verified,
    is_published = excluded.is_published,
    updated_at = CURRENT_TIMESTAMP;

INSERT INTO app_sources (app_id, backend, package_id, repository, architecture, priority)
SELECT id, 'flatpak', component_id, 'flathub', '', 10 FROM apps
WHERE component_id IN (
    'org.mozilla.firefox', 'org.kde.krita', 'com.valvesoftware.Steam',
    'org.libreoffice.LibreOffice', 'org.videolan.VLC', 'org.gimp.GIMP',
    'com.obsproject.Studio', 'org.blender.Blender', 'com.discordapp.Discord',
    'com.spotify.Client', 'com.heroicgameslauncher.hgl', 'com.usebottles.bottles',
    'com.visualstudio.code', 'org.gnome.Calculator', 'org.kde.kdenlive',
    'com.jetbrains.CLion', 'com.jetbrains.Toolbox'
)
ON CONFLICT (app_id, backend, package_id) DO UPDATE SET
    repository = excluded.repository,
    architecture = excluded.architecture,
    priority = excluded.priority,
    is_available = 1;
