import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    width: 1100
    height: 720
    minimumWidth: 760
    minimumHeight: 520

    visible: true
    title: qsTr("Plaza")

    property int currentSection: 0
    property bool hasCheckedForUpdates: false
    property string pendingRemoveRemote: ""

    function showSection(section) {
        currentSection = section
        softwarePage.showingInstalled = section === 1
        if (section === 1) softwarePage.showInstalledApplications()
        if (section === 3) flatpakBackend.refreshSources()
    }

    header: Controls.ToolBar {
        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                Layout.topMargin: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "system-software-install"
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                }

                Controls.Label {
                    text: qsTr("Plaza")
                    font.bold: true
                    font.pointSize: 16
                }

                Item { Layout.fillWidth: true }

                Controls.ToolButton {
                    icon.name: "search"
                    text: qsTr("Search")
                    display: Controls.AbstractButton.IconOnly
                    Accessible.name: qsTr("Search applications")
                    onClicked: {
                        root.showSection(0)
                        searchField.forceActiveFocus()
                    }
                }

                Controls.ToolButton {
                    icon.name: "system-software-install"
                    text: qsTr("Activity")
                    display: Controls.AbstractButton.IconOnly
                    Accessible.name: qsTr("Installation activity")
                    onClicked: activityDialog.open()
                }

                Controls.ToolButton {
                    icon.name: "settings-configure"
                    text: qsTr("Settings")
                    display: Controls.AbstractButton.IconOnly
                    Accessible.name: qsTr("Plaza settings and about")
                    onClicked: settingsDialog.open()
                }
            }

            Controls.TabBar {
                Layout.alignment: Qt.AlignHCenter
                currentIndex: root.currentSection
                background: Item {}

                Controls.TabButton {
                    text: qsTr("Discover")
                    icon.name: "search"
                    onClicked: root.showSection(0)
                }
                Controls.TabButton {
                    text: qsTr("Installed")
                    icon.name: "view-list-icons"
                    onClicked: root.showSection(1)
                }
                Controls.TabButton {
                    text: qsTr("Updates")
                    icon.name: "system-software-update"
                    onClicked: root.showSection(2)
                }
                Controls.TabButton {
                    text: qsTr("Sources")
                    icon.name: "server-database"
                    onClicked: root.showSection(3)
                }
            }
        }
    }

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.globalToolBar.showNavigationButtons:
        Kirigami.ApplicationHeaderStyle.NoNavigationButtons

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: softwarePage

        // The content heading supplies this label; leaving the page title empty
        // avoids reserving toolbar space beside the centered navigation.
        title: ""
        property bool showingInstalled: false
        readonly property var activeSearch: showingInstalled ? installedSearch : catalogSearch
        readonly property var activePage: showingInstalled ? installedPage : catalogPage

        function showInstalledApplications() {
            showingInstalled = true
            installedApps.reload()
        }

        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                visible: root.currentSection < 2
                text: softwarePage.showingInstalled
                    ? qsTr("Installed applications")
                    : qsTr("Discover applications")
                level: 1
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Controls.Label {
                visible: root.currentSection < 2
                text: softwarePage.showingInstalled
                    ? qsTr(
                        "Apps detected from Flatpak and desktop entries " +
                        "installed by Bazaar, Discover, or you."
                    )
                    : qsTr(
                        "Explore Plaza and your configured Flatpak sources. Flatpak installations use your user sources."
                    )
                opacity: 0.72
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                visible: root.currentSection < 2
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Controls.TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: softwarePage.showingInstalled
                        ? qsTr("Search installed applications")
                        : qsTr("Search applications by name, description, or ID")
                    Accessible.name: placeholderText
                    text: softwarePage.activeSearch.searchText
                    onTextEdited: softwarePage.activeSearch.searchText = text
                    selectByMouse: true
                }

                Controls.Button {
                    text: qsTr("Clear")
                    enabled: searchField.text.length > 0
                    onClicked: {
                        softwarePage.activeSearch.searchText = ""
                        searchField.forceActiveFocus()
                    }
                }
            }

            Shortcut {
                sequences: [ StandardKey.Find ]
                onActivated: {
                    root.showSection(0)
                    searchField.forceActiveFocus()
                }
            }

            Controls.Label {
                visible: root.currentSection < 2
                Layout.fillWidth: true
                text: softwarePage.activeSearch.searchText.trim().length > 0
                    ? qsTr("%n result(s)", "", softwarePage.activeSearch.count)
                    : softwarePage.showingInstalled
                        ? qsTr("Your applications")
                        : qsTr("All applications")
                opacity: 0.72
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: root.currentSection < 2 && softwarePage.activeSearch.count === 0 && (softwarePage.showingInstalled || !flatpakCatalog.loading)
                text: softwarePage.activeSearch.searchText.trim().length > 0
                    ? qsTr("No applications match your search. Try a different name or clear the search.")
                    : softwarePage.showingInstalled
                        ? qsTr("No installed applications were found.")
                        : qsTr("No applications were found. Configure a Flatpak source and make its app catalog available, or connect the Plaza catalog database.")
                wrapMode: Text.WordWrap
                opacity: 0.72
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.currentSection < 4 && flatpakBackend.errorText.length > 0
                type: Kirigami.MessageType.Error
                text: flatpakBackend.errorText
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.currentSection < 3 && packageBackend.errorText.length > 0
                type: Kirigami.MessageType.Error
                text: packageBackend.errorText
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.currentSection === 0 && !catalogDatabaseAvailable && !softwarePage.showingInstalled
                type: Kirigami.MessageType.Warning
                text: qsTr("The Plaza catalog database is unavailable. Showing cached Flatpak sources instead.")
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: root.currentSection === 0 && flatpakCatalog.loading && !softwarePage.showingInstalled
                text: qsTr("Loading software sources…")
                opacity: 0.72
            }

            GridLayout {
                visible: root.currentSection < 2
                Layout.fillWidth: true
                columns: Math.max(1, Math.min(4, Math.floor(width / 220)))
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.largeSpacing

                Repeater {
                    model: softwarePage.showingInstalled
                        ? installedPage
                        : catalogPage

                    delegate: Kirigami.Card {
                    required property int appId
                    required property string componentId
                    required property string displayName
                    required property string summary
                    required property string description
                    required property string iconName
                    required property string backend
                    required property string packageId
                    required property string repository
                    required property bool verified
                    required property var sources

                    readonly property var selectedSource: sources.length > 0
                        ? sources[Math.max(0, sourcePicker.currentIndex)] : null
                    readonly property string selectedBackend: selectedSource ? selectedSource.backend : backend
                    readonly property string selectedPackageId: selectedSource ? selectedSource.packageId : packageId
                    readonly property string selectedRepository: selectedSource ? selectedSource.repository : repository

                    readonly property bool flatpakRow:
                        selectedBackend.toLowerCase() === "flatpak"
                    readonly property bool nativeRow: packageBackend.supports(selectedBackend)
                    readonly property bool flatpakInstalled:
                        flatpakRow && flatpakBackend.cacheVersion > 0
                        && flatpakBackend.isInstalled(selectedPackageId)
                    readonly property bool installedFromAnotherSource:
                        backend !== "local" && installedApps.version > 0
                        && installedApps.isInstalled(
                            componentId,
                            selectedPackageId,
                            displayName
                        )
                    readonly property bool alreadyInstalled:
                        backend === "local" || flatpakInstalled
                        || installedFromAnotherSource || (nativeRow && packageBackend.cacheVersion > 0
                            && packageBackend.isInstalled(selectedPackageId))
                    readonly property bool installingThisApp:
                        (flatpakRow && flatpakBackend.busy && flatpakBackend.currentAppId === selectedPackageId)
                        || (nativeRow && packageBackend.busy && packageBackend.currentPackage === selectedPackageId)
                    readonly property bool anotherAppInstalling:
                        (flatpakBackend.busy || packageBackend.busy) && !installingThisApp

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 240

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true

                            Kirigami.Icon {
                                source: iconName.length > 0
                                    ? iconName
                                    : "application-x-executable"

                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Controls.Label {
                                        text: displayName
                                        font.bold: true
                                        font.pointSize: 13
                                    }

                                    Kirigami.Icon {
                                        visible: verified
                                        source: "emblem-verified"
                                        implicitWidth: 16
                                        implicitHeight: 16
                                    }
                                }

                                Controls.Label {
                                    text: summary
                                    opacity: 0.68
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        Controls.ComboBox {
                            id: sourcePicker
                            Layout.fillWidth: true
                            visible: sources.length > 1
                            enabled: !flatpakBackend.busy && !packageBackend.busy
                            Accessible.name: qsTr("Installation source for %1").arg(displayName)
                            model: sources.map(function(source) {
                                const name = source.repository.length > 0 ? source.repository : source.backend
                                const format = source.backend === "flatpak" ? "Flatpak" : source.backend.toUpperCase()
                                return name + " — " + format + (source.backend === "flatpak" || packageBackend.supports(source.backend) ? "" : qsTr(" (coming soon)"))
                            })
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Controls.Label {
                                text: selectedBackend.length === 0
                                    ? qsTr("Coming soon")
                                    : selectedRepository.length > 0
                                        ? selectedBackend + " · " + selectedRepository
                                        : selectedBackend

                                opacity: 0.65
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Controls.Button {
                                text: installingThisApp
                                        ? qsTr("Cancel")
                                        : flatpakInstalled
                                            ? qsTr("Open")
                                            : alreadyInstalled
                                                ? qsTr("Installed")
                                                : !flatpakRow && !nativeRow
                                                    ? qsTr("Coming soon")
                                                    : qsTr("Install")
                                icon.name: installingThisApp
                                        ? "dialog-cancel"
                                        : flatpakInstalled
                                            ? "system-run"
                                            : alreadyInstalled
                                                ? "emblem-ok"
                                                : !flatpakRow && !nativeRow
                                                    ? "dialog-information"
                                                    : "download"
                                enabled: installingThisApp || flatpakInstalled || (
                                    !alreadyInstalled && (flatpakRow || nativeRow)
                                    && !anotherAppInstalling
                                )

                                onClicked: {
                                    if (installingThisApp) {
                                        if (flatpakRow) flatpakBackend.cancel()
                                        else packageBackend.cancel()
                                    } else if (flatpakInstalled) {
                                        flatpakBackend.launch(selectedPackageId)
                                    } else if (nativeRow) {
                                        packageBackend.install(selectedBackend, selectedRepository, selectedPackageId)
                                    } else {
                                        flatpakBackend.install(
                                            selectedRepository,
                                            selectedPackageId
                                        )
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: installingThisApp
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Controls.Label {
                                text: flatpakRow ? flatpakBackend.statusText : packageBackend.statusText
                                opacity: 0.72
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Controls.ProgressBar {
                                from: 0
                                to: 100
                                value: flatpakRow ? flatpakBackend.progress : packageBackend.progress
                                Layout.fillWidth: true
                            }
                        }
                    }
                    }
                }
            }

            ColumnLayout {
                visible: root.currentSection === 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    text: qsTr("Updates")
                    level: 1
                }

                Controls.Label {
                    Layout.fillWidth: true
                    text: qsTr("Check your user Flatpak installation and this system’s native package manager for available updates.")
                    opacity: 0.72
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Button {
                        text: qsTr("Check for updates")
                        icon.name: "view-refresh"
                        enabled: !flatpakBackend.busy && !packageBackend.busy
                        onClicked: {
                            root.hasCheckedForUpdates = true
                            flatpakBackend.checkUpdates()
                            if (packageBackend.supports("dnf") || packageBackend.supports("apt"))
                                packageBackend.checkUpdates()
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Controls.Label {
                        text: qsTr("%n update(s)", "", flatpakBackend.updates.length + packageBackend.updates.length)
                        opacity: 0.72
                    }
                }

                Controls.Label {
                    Layout.fillWidth: true
                    visible: flatpakBackend.busy || packageBackend.busy
                    text: flatpakBackend.busy ? flatpakBackend.statusText : packageBackend.statusText
                    opacity: 0.72
                }

                Controls.ProgressBar {
                    Layout.fillWidth: true
                    visible: flatpakBackend.busy || packageBackend.busy
                    indeterminate: (flatpakBackend.busy
                                    && flatpakBackend.currentAppId === "updates"
                                    && flatpakBackend.statusText.startsWith(qsTr("Checking")))
                        || (packageBackend.busy && packageBackend.statusText.startsWith(qsTr("Checking")))
                    from: 0
                    to: 100
                    value: flatpakBackend.busy ? flatpakBackend.progress : packageBackend.progress
                }

                Repeater {
                    model: flatpakBackend.updates
                    delegate: Kirigami.Card {
                        required property string appId
                        required property string name
                        required property string kind
                        required property string branch
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            Kirigami.Icon {
                                source: kind === "app" ? "application-x-executable" : "package-x-generic"
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Controls.Label { text: name; font.bold: true }
                                Controls.Label {
                                    text: qsTr("Flatpak · %1 · %2").arg(kind).arg(branch)
                                    opacity: 0.72
                                }
                            }
                        }
                    }
                }

                Controls.Button {
                    Layout.alignment: Qt.AlignRight
                    visible: flatpakBackend.updates.length > 0
                    enabled: !flatpakBackend.busy && !packageBackend.busy
                    text: qsTr("Update Flatpaks")
                    icon.name: "system-software-update"
                    onClicked: flatpakBackend.updateAll()
                }

                Repeater {
                    model: packageBackend.updates
                    delegate: Kirigami.Card {
                        required property string name
                        required property string packageId
                        required property string summary
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            Kirigami.Icon {
                                source: "package-x-generic"
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Controls.Label { text: name; font.bold: true }
                                Controls.Label {
                                    text: summary.length > 0 ? summary : packageId
                                    opacity: 0.72
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                Controls.Button {
                    Layout.alignment: Qt.AlignRight
                    visible: packageBackend.updates.length > 0
                    enabled: !flatpakBackend.busy && !packageBackend.busy
                    text: qsTr("Update system packages")
                    icon.name: "system-software-update"
                    onClicked: packageBackend.updateAll()
                }

                Kirigami.PlaceholderMessage {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !flatpakBackend.busy && !packageBackend.busy
                        && root.hasCheckedForUpdates
                        && flatpakBackend.cacheReady && flatpakBackend.updates.length === 0
                        && packageBackend.updates.length === 0
                        && flatpakBackend.errorText.length === 0 && packageBackend.errorText.length === 0
                    icon.name: "system-software-update"
                    text: flatpakBackend.cacheReady ? qsTr("No available updates") : qsTr("Check for software updates")
                    explanation: flatpakBackend.cacheReady
                        ? qsTr("Flatpak and supported system package sources are up to date.")
                        : qsTr("Plaza will check your configured user Flatpak installation and supported system package manager.")
                }
            }

            ColumnLayout {
                visible: root.currentSection === 3
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    text: qsTr("Software sources")
                    level: 1
                }

                Controls.Label {
                    Layout.fillWidth: true
                    text: qsTr("Manage Flatpak sources for your user installation. Plaza verifies signed sources and never needs administrator access for these changes.")
                    opacity: 0.72
                    wrapMode: Text.WordWrap
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: flatpakBackend.errorText.length > 0
                    type: Kirigami.MessageType.Error
                    text: flatpakBackend.errorText
                }

                RowLayout {
                    Layout.fillWidth: true
                    Controls.Label {
                        Layout.fillWidth: true
                        text: flatpakBackend.busy && flatpakBackend.currentAppId === "sources"
                            ? flatpakBackend.statusText
                            : flatpakBackend.sourceStatusText
                        opacity: 0.72
                        elide: Text.ElideRight
                    }
                    Controls.Button {
                        text: qsTr("Add source")
                        icon.name: "list-add"
                        enabled: !flatpakBackend.busy
                        onClicked: addSourceDialog.open()
                    }
                }

                Controls.ProgressBar {
                    Layout.fillWidth: true
                    visible: flatpakBackend.busy && flatpakBackend.currentAppId === "sources"
                    indeterminate: true
                }

                Repeater {
                    model: flatpakBackend.remotes

                    delegate: Kirigami.Card {
                        required property string remoteName
                        required property string title
                        required property string url
                        required property bool remoteEnabled
                        required property bool gpgVerified

                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            Kirigami.Icon {
                                source: "flatpak"
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Controls.Label {
                                    text: title.length > 0 ? title : remoteName
                                    font.bold: true
                                }
                                Controls.Label {
                                    text: qsTr("%1 · %2").arg(remoteName).arg(gpgVerified ? qsTr("GPG verified") : qsTr("Not verified"))
                                    opacity: 0.72
                                }
                                Controls.Label {
                                    Layout.fillWidth: true
                                    text: url
                                    opacity: 0.62
                                    elide: Text.ElideMiddle
                                }
                            }
                            Controls.Switch {
                                text: remoteEnabled ? qsTr("Enabled") : qsTr("Disabled")
                                checked: remoteEnabled
                                enabled: !flatpakBackend.busy
                                onToggled: flatpakBackend.setSourceEnabled(remoteName, checked)
                            }
                            Controls.Button {
                                text: qsTr("Remove")
                                icon.name: "edit-delete"
                                enabled: !flatpakBackend.busy
                                onClicked: {
                                    root.pendingRemoveRemote = remoteName
                                    removeSourceDialog.open()
                                }
                            }
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    Layout.fillWidth: true
                    visible: flatpakBackend.remotes.length === 0 && !flatpakBackend.busy
                    icon.name: "server-database"
                    text: qsTr("No user Flatpak sources configured")
                    explanation: qsTr("Add a signed .flatpakrepo source to discover and install Flatpak apps.")
                }

                Kirigami.Heading {
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    text: qsTr("System package sources")
                    level: 2
                }

                Controls.Label {
                    Layout.fillWidth: true
                    text: qsTr("APT, DNF, and other system repositories are managed by your operating system. Plaza can install compatible packages from them, but does not change system repository configuration.")
                    opacity: 0.72
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: appCatalog.sourceList.filter(function(source) { return source.backend !== "flatpak" })
                    delegate: Kirigami.Card {
                        required property string backend
                        required property string repository
                        required property int appCount
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            Kirigami.Icon {
                                source: "server-database"
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Controls.Label { text: repository.length > 0 ? repository : backend.toUpperCase(); font.bold: true }
                                Controls.Label { text: backend.toUpperCase() + qsTr(" · managed by the operating system"); opacity: 0.72 }
                            }
                            Controls.Label { text: qsTr("%n catalog app(s)", "", appCount); opacity: 0.72 }
                        }
                    }
                }
            }

            Controls.Button {
                Layout.alignment: Qt.AlignHCenter
                visible: root.currentSection < 2 && softwarePage.activeSearch.count > softwarePage.activePage.limit
                text: qsTr("Show more applications")
                onClicked: softwarePage.activePage.showMore()
            }

        }
    }

    Kirigami.Dialog {
        id: addSourceDialog
        title: qsTr("Add Flatpak source")
        standardButtons: Kirigami.Dialog.NoButton
        preferredWidth: Kirigami.Units.gridUnit * 28

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.mediumSpacing
            Controls.Label {
                Layout.fillWidth: true
                text: qsTr("Add a signed Flatpak repository using its HTTPS .flatpakrepo link. Plaza keeps GPG verification enabled.")
                wrapMode: Text.WordWrap
            }
            Controls.TextField {
                id: sourceNameField
                Layout.fillWidth: true
                placeholderText: qsTr("Source name, for example flathub")
                selectByMouse: true
            }
            Controls.TextField {
                id: sourceUrlField
                Layout.fillWidth: true
                placeholderText: qsTr("https://example.org/repository.flatpakrepo")
                selectByMouse: true
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Controls.Button { text: qsTr("Cancel"); onClicked: addSourceDialog.close() }
                Controls.Button {
                    text: qsTr("Add source")
                    icon.name: "list-add"
                    enabled: !flatpakBackend.busy && sourceNameField.text.trim().length > 0 && sourceUrlField.text.trim().length > 0
                    onClicked: {
                        flatpakBackend.addSource(sourceNameField.text, sourceUrlField.text)
                        addSourceDialog.close()
                        sourceNameField.text = ""
                        sourceUrlField.text = ""
                    }
                }
            }
        }
    }

    Kirigami.Dialog {
        id: removeSourceDialog
        title: qsTr("Remove Flatpak source?")
        standardButtons: Kirigami.Dialog.NoButton
        preferredWidth: Kirigami.Units.gridUnit * 26

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.mediumSpacing
            Controls.Label {
                Layout.fillWidth: true
                text: qsTr("Remove %1 from your user installation? This only removes the source configuration; apps already installed from it are left in place.").arg(root.pendingRemoveRemote)
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Controls.Button { text: qsTr("Cancel"); onClicked: removeSourceDialog.close() }
                Controls.Button {
                    text: qsTr("Remove source")
                    icon.name: "edit-delete"
                    enabled: !flatpakBackend.busy
                    onClicked: {
                        flatpakBackend.removeSource(root.pendingRemoveRemote)
                        removeSourceDialog.close()
                    }
                }
            }
        }
    }

    Kirigami.Dialog {
        id: activityDialog
        title: qsTr("Installation activity")
        standardButtons: Kirigami.Dialog.NoButton
        preferredWidth: Kirigami.Units.gridUnit * 24

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.mediumSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: flatpakBackend.busy
                    ? qsTr("%1 — %2").arg(flatpakBackend.currentAppId).arg(flatpakBackend.statusText)
                    : packageBackend.busy
                        ? qsTr("%1 — %2").arg(packageBackend.currentPackage).arg(packageBackend.statusText)
                        : qsTr("There are no active installation tasks.")
                wrapMode: Text.WordWrap
            }

            Controls.ProgressBar {
                Layout.fillWidth: true
                visible: flatpakBackend.busy || packageBackend.busy
                from: 0
                to: 100
                value: flatpakBackend.busy ? flatpakBackend.progress : packageBackend.progress
            }

            Controls.Button {
                text: qsTr("Cancel installation")
                visible: flatpakBackend.busy || packageBackend.busy
                onClicked: flatpakBackend.busy ? flatpakBackend.cancel() : packageBackend.cancel()
            }
        }
    }

    Kirigami.Dialog {
        id: settingsDialog
        title: qsTr("Plaza settings")
        standardButtons: Kirigami.Dialog.Close
        preferredWidth: Kirigami.Units.gridUnit * 24

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.mediumSpacing
            Kirigami.Heading {
                text: qsTr("About Plaza")
                level: 2
            }
            Controls.Label {
                Layout.fillWidth: true
                text: qsTr("A unified place to discover and install software from sources compatible with your system.")
                wrapMode: Text.WordWrap
            }
            Controls.Label {
                Layout.fillWidth: true
                text: qsTr("Flatpak installs are scoped to your user account. Native packages use the configured system package manager.")
                wrapMode: Text.WordWrap
                opacity: 0.72
            }
            Controls.Label {
                Layout.fillWidth: true
                text: qsTr("Version %1").arg(plazaVersion)
                opacity: 0.72
            }
        }
    }
}
