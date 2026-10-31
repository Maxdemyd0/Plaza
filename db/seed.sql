INSERT INTO apps (
    component_id,
    name,
    summary,
    description,
    icon_name,
    homepage_url,
    is_verified,
    is_published
)
VALUES
    (
        'org.kde.krita',
        'Krita',
        'Digital painting and illustration',
        'A professional painting program created by the KDE community.',
        'krita',
        'https://krita.org',
        TRUE,
        TRUE
    ),
    (
        'com.valvesoftware.Steam',
        'Steam',
        'Games and questionable purchase decisions',
        'A game distribution and community platform.',
        'steam',
        'https://store.steampowered.com',
        TRUE,
        TRUE
    ),
    (
        'com.jetbrains.CLion',
        'CLion',
        'C and C++ development environment',
        'A cross-platform C and C++ IDE from JetBrains.',
        'clion',
        'https://www.jetbrains.com/clion/',
        TRUE,
        TRUE
    ),
    (
        'com.jetbrains.Toolbox',
        'JetBrains Toolbox',
        'Manage your JetBrains development tools',
        'Install, update, and manage JetBrains IDEs and developer tools from one place.',
        'applications-development',
        'https://www.jetbrains.com/toolbox-app/',
        TRUE,
        TRUE
    )
    ON CONFLICT (component_id) DO UPDATE SET
    name = EXCLUDED.name,
                                      summary = EXCLUDED.summary,
                                      description = EXCLUDED.description,
                                      icon_name = EXCLUDED.icon_name,
                                      homepage_url = EXCLUDED.homepage_url,
                                      is_verified = EXCLUDED.is_verified,
                                      is_published = EXCLUDED.is_published,
                                      updated_at = CURRENT_TIMESTAMP;


INSERT INTO app_sources (
    app_id,
    backend,
    package_id,
    repository,
    architecture,
    priority
)
SELECT
    id,
    'flatpak',
    'org.kde.krita',
    'flathub',
    'x86_64',
    10
FROM apps
WHERE component_id = 'org.kde.krita'
    ON CONFLICT DO NOTHING;


INSERT INTO app_sources (
    app_id,
    backend,
    package_id,
    repository,
    architecture,
    priority
)
SELECT
    id,
    'dnf',
    'krita',
    'fedora',
    'x86_64',
    20
FROM apps
WHERE component_id = 'org.kde.krita'
    ON CONFLICT DO NOTHING;


INSERT INTO app_sources (
    app_id,
    backend,
    package_id,
    repository,
    architecture,
    priority
)
SELECT
    id,
    'flatpak',
    'com.valvesoftware.Steam',
    'flathub',
    'x86_64',
    10
FROM apps
WHERE component_id = 'com.valvesoftware.Steam'
    ON CONFLICT DO NOTHING;


INSERT INTO app_sources (
    app_id,
    backend,
    package_id,
    repository,
    architecture,
    priority
)
SELECT
    id,
    'dnf',
    'steam',
    'rpm-fusion-nonfree',
    'x86_64',
    20
FROM apps
WHERE component_id = 'com.valvesoftware.Steam'
    ON CONFLICT DO NOTHING;
