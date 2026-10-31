# Plaza

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Plaza is a Qt 6 / C++20 / QML software center. Version 1.1.1 provides app
discovery and search, popularity ordering when catalog scores are available,
installed-app matching, Flatpak installation and updates, and PackageKit-backed
APT package operations on supported Debian-based systems.

Project overview and user documentation: <https://maxdemyd0.github.io/Plaza/>.

## Build

Install the development packages for Qt 6 (Quick, Quick Controls, SQL with the
SQLite driver, Concurrent, Network, DBus, and the Kirigami QML module),
libflatpak, zlib, CMake, and Ninja. Then build and run the project:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/plaza
cpack --config build/CPackConfig.cmake -G DEB -B build/packages
```

To install Plaza and its desktop launcher under a chosen prefix:

```sh
cmake --install build --prefix "$HOME/.local"
```

Log out and back in, or refresh the desktop application cache, if the launcher
does not appear immediately.

Debian and Orbis OS users can download the latest `.deb` from the
[GitHub releases page](https://github.com/Maxdemyd0/Plaza/releases/latest).
Release tags also trigger the repository workflow to build Debian packages for
amd64, i386, arm64, and armhf, then attach them automatically. The amd64 build
works on both Intel 64-bit and AMD64 PCs. Debian 13 treats i386 as legacy
32-bit compatibility for amd64 systems, not as a supported standalone install.

## App catalog

The starter catalog is bundled with Plaza and initialized automatically in a
small SQLite database in the user's application-data folder. It needs no
PostgreSQL server, account, credentials, or first-run setup. Flatpak AppStream
metadata from the user's configured remotes is merged into those listings
when available; the bundled catalog remains available offline. The packaged
catalog seed is `db/catalog.sql`.

## Installation sources and platform support

- Flatpak installs, updates, and source configuration use the current user's
  Flatpak installation. Adding a source requires an HTTPS `.flatpakrepo` file
  with GPG verification enabled.
- If a selected app uses Flathub and the user remote is missing, Plaza adds the
  official Flathub repository to that user's installation during the install
  flow, with its signing key and verification enabled. No separate setup step
  is required; Plaza never changes system-wide sources or silently substitutes
  another repository.
- APT catalog entries are installed through PackageKit on Debian/Ubuntu-based
  systems when PackageKit is installed and the service supports the operation.
- DNF sources are not used on Debian-based systems.
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

## Plaza updates

Plaza checks the public GitHub Releases API for a newer stable release. On
supported Debian systems, the update action downloads the matching `.deb`,
checks its SHA-256 digest against GitHub's release metadata, then asks PackageKit
to install it with normal system authorization. Restart Plaza after installation
to load the updated version. No GitHub token or account is needed.

## Release checklist

Before publishing a distribution package, verify PackageKit/Flatpak policy on
the target OS, run the tests on that OS, and build and test the package through
its intended distribution channel. GitHub releases provide Debian packages;
Plaza does not yet publish a signed APT repository. Plaza is MIT licensed.

## License

Plaza is licensed under the [MIT License](LICENSE). Source code and issues are
hosted at [github.com/Maxdemyd0/Plaza](https://github.com/Maxdemyd0/Plaza).
