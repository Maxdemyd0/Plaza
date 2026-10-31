# Plaza

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Plaza is a Qt 6 / C++20 / QML software center. Version 1.0.0 provides app
discovery and search, popularity ordering when catalog scores are available,
installed-app matching, Flatpak installation and updates, and PackageKit-backed
APT/DNF package operations where the host supports them.

## Build

Install the development packages for Qt 6 (Quick, Quick Controls, SQL with the
PostgreSQL driver, Concurrent, Network, DBus, and the Kirigami QML module),
libflatpak, PostgreSQL client libraries, zlib, CMake, and Ninja. Then build and
run the project:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/plaza
```

To install Plaza and its desktop launcher under a chosen prefix:

```sh
cmake --install build --prefix "$HOME/.local"
```

Log out and back in, or refresh the desktop application cache, if the launcher
does not appear immediately.

## Catalog database

The PostgreSQL catalog is optional at runtime: cached Flatpak app metadata and
the Installed page remain available if the database cannot be reached. To
enable the curated Plaza catalog, provision PostgreSQL separately, create the
`plaza` database and a role allowed to connect, then apply:

```sh
psql -d plaza -f db/schema.sql
psql -d plaza -f db/seed.sql
```

Configure the connection with these optional environment variables:

| Variable | Default |
| --- | --- |
| `PLAZA_DB_NAME` | `plaza` |
| `PLAZA_DB_USER` | current login name (`$USER`) |
| `PLAZA_DB_HOST` | PostgreSQL local socket |
| `PLAZA_DB_PORT` | `5432` |
| `PLAZA_DB_PASSWORD` | empty; use PostgreSQL's configured authentication |

Do not put database passwords in the source tree or seed files. In production,
deploy and back up the catalog database independently from Plaza clients.

## Installation sources and platform support

- Flatpak installs, updates, and source configuration use the current user's
  Flatpak installation. Adding a source requires an HTTPS `.flatpakrepo` file
  with GPG verification enabled.
- APT and DNF catalog entries are installed through PackageKit only when the
  detected operating system and PackageKit service support that backend.
  Plaza does not edit system repository configuration.
- rpm-ostree and other package managers are not installable in this release;
  unsupported catalog sources are shown as unavailable.
- Installed applications are discovered from desktop entries and matched to
  store metadata when possible. Their native launchers remain owned by the
  system or the installer that created them.

Package-manager and Flatpak operations require the corresponding system
services, configured repositories, network access, and user authorization. A
successful compile does not verify those host services or perform a package
transaction.

## Release checklist

Before publishing a distribution package, verify the catalog service and
PackageKit/Flatpak policy on the target OS, run the tests on that OS, and build
and test the package through its intended distribution channel. Plaza is MIT
licensed, but this repository does not provide signed distro-specific packages.

## License

Plaza is licensed under the [MIT License](LICENSE). Source code and issues are
hosted at [github.com/Maxdemyd0/Plaza](https://github.com/Maxdemyd0/Plaza).
