pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs

Dialog {
    id: manager
    objectName: "snippetManager"
    required property var backendApi
    property var builtInTriggers: []
    property string currentCourse: ""
    property int editingIndex: -1
    property string editingId: ""
    property string statusText: ""
    signal snippetsChanged()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, parent ? parent.width - 32 : 760)
    height: Math.min(760, parent ? parent.height - 32 : 760)
    modal: true
    title: "Custom Tab snippets"
    closePolicy: Popup.CloseOnEscape

    ListModel { id: snippetRows }

    function splitList(text) {
        var parts = text.split(",")
        var result = []
        for (var i = 0; i < parts.length; ++i) {
            var value = parts[i].trim()
            if (value.length) result.push(value)
        }
        return result
    }

    function entryFromRow(row) {
        return {
            id: row.id || "",
            name: row.name || "",
            trigger: row.trigger || "",
            aliases: splitList(row.aliasText || ""),
            template: row.template || "",
            allCourses: row.allCourses === undefined ? true : row.allCourses,
            courses: splitList(row.courseText || ""),
            enabled: row.isEnabled === undefined ? true : row.isEnabled
        }
    }

    function entriesFromModel(replacement, replacementIndex) {
        var result = []
        for (var i = 0; i < snippetRows.count; ++i)
            result.push(i === replacementIndex ? replacement
                                               : entryFromRow(snippetRows.get(i)))
        if (replacementIndex < 0) result.push(replacement)
        return result
    }

    function loadEntries(entries) {
        snippetRows.clear()
        for (var i = 0; i < entries.length; ++i) {
            var item = entries[i]
            snippetRows.append({
                id: item.id || "",
                name: item.name || item.trigger || "",
                trigger: item.trigger || "",
                aliasText: (item.aliases || []).join(", "),
                template: item.template || "",
                allCourses: item.allCourses === undefined ? true : item.allCourses,
                courseText: (item.courses || []).join(", "),
                isEnabled: item.enabled === undefined ? true : item.enabled
            })
        }
    }

    function reload() {
        loadEntries(backendApi.customSnippets())
        snippetsChanged()
        if (snippetRows.count) editSnippet(0)
        else newSnippet()
    }

    function openManager() {
        open()
    }

    function newSnippet() {
        editingIndex = -1
        editingId = ""
        nameField.text = ""
        triggerField.text = ""
        aliasesField.text = ""
        templateField.text = ""
        enabledSwitch.checked = true
        allCoursesSwitch.checked = true
        coursesField.text = currentCourse
        statusText = ""
        Qt.callLater(triggerField.forceActiveFocus)
    }

    function editSnippet(index) {
        if (index < 0 || index >= snippetRows.count) return
        var item = snippetRows.get(index)
        editingIndex = index
        editingId = item.id || ""
        nameField.text = item.name || ""
        triggerField.text = item.trigger || ""
        aliasesField.text = item.aliasText || ""
        templateField.text = item.template || ""
        enabledSwitch.checked = item.isEnabled
        allCoursesSwitch.checked = item.allCourses
        coursesField.text = item.courseText || ""
        statusText = ""
    }

    function currentEntry() {
        return {
            id: editingId,
            name: nameField.text,
            trigger: triggerField.text,
            aliases: splitList(aliasesField.text),
            template: templateField.text,
            allCourses: allCoursesSwitch.checked,
            courses: splitList(coursesField.text),
            enabled: enabledSwitch.checked
        }
    }

    function saveCurrent() {
        var result = backendApi.replaceCustomSnippets(
                    entriesFromModel(currentEntry(), editingIndex))
        if (!result.saved) {
            statusText = result.error || "Could not save the snippet"
            return
        }
        loadEntries(result.entries || backendApi.customSnippets())
        snippetsChanged()
        var nextIndex = editingIndex < 0 ? snippetRows.count - 1 : editingIndex
        editSnippet(Math.max(0, Math.min(snippetRows.count - 1, nextIndex)))
        statusText = "Saved"
    }

    function duplicateCurrent() {
        if (editingIndex < 0) return
        var item = snippetRows.get(editingIndex)
        editingIndex = -1
        editingId = ""
        nameField.text = (item.name || item.trigger) + " copy"
        triggerField.text = (item.trigger || "") + "copy"
        aliasesField.text = ""
        templateField.text = item.template || ""
        enabledSwitch.checked = item.isEnabled
        allCoursesSwitch.checked = item.allCourses
        coursesField.text = item.courseText || ""
        statusText = "Change the trigger, then save"
        Qt.callLater(triggerField.forceActiveFocus)
    }

    function deleteCurrent() {
        if (editingIndex < 0) return
        var remaining = []
        for (var i = 0; i < snippetRows.count; ++i)
            if (i !== editingIndex) remaining.push(entryFromRow(snippetRows.get(i)))
        var result = backendApi.replaceCustomSnippets(remaining)
        if (!result.saved) {
            statusText = result.error || "Could not delete the snippet"
            return
        }
        loadEntries(result.entries || [])
        snippetsChanged()
        if (snippetRows.count) editSnippet(Math.min(editingIndex, snippetRows.count - 1))
        else newSnippet()
        statusText = "Deleted"
    }

    function hasBuiltInConflict() {
        var values = [triggerField.text.trim().toLowerCase()].concat(
                    splitList(aliasesField.text.toLowerCase()))
        for (var i = 0; i < values.length; ++i)
            if (builtInTriggers.indexOf(values[i]) >= 0) return true
        return false
    }

    onOpened: reload()

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: "Custom snippets override built-ins in their scope. All courses is the default."
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.2)
            radius: 5

            ListView {
                id: snippetList
                objectName: "customSnippetList"
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                model: snippetRows
                delegate: ItemDelegate {
                    required property int index
                    required property string name
                    required property string trigger
                    required property bool allCourses
                    required property string courseText
                    required property bool isEnabled
                    width: ListView.view.width
                    highlighted: index === manager.editingIndex
                    text: (isEnabled ? "" : "Disabled · ") + name + "  —  " + trigger
                          + "  ·  " + (allCourses ? "All courses" : courseText)
                    onClicked: manager.editSnippet(index)
                }
                ScrollBar.vertical: ScrollBar { }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button { text: "New"; onClicked: manager.newSnippet() }
            Button { text: "Duplicate"; enabled: manager.editingIndex >= 0; onClicked: manager.duplicateCurrent() }
            Button { text: "Delete"; enabled: manager.editingIndex >= 0; onClicked: deleteConfirm.open() }
            Item { Layout.fillWidth: true }
            Button { text: "Import…"; onClicked: importConfirm.open() }
            Button { text: "Export…"; enabled: snippetRows.count > 0; onClicked: exportDialog.open() }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: manager.width >= 620 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Name" }
                TextField { id: nameField; objectName: "snippetNameField"; Layout.fillWidth: true; placeholderText: "Basis vectors" }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "Trigger" }
                TextField { id: triggerField; objectName: "snippetTriggerField"; Layout.fillWidth: true; placeholderText: "basis" }
            }
        }

        Label { text: "Aliases, separated by commas" }
        TextField {
            id: aliasesField
            objectName: "snippetAliasesField"
            Layout.fillWidth: true
            placeholderText: "span, bas"
        }

        Label { text: "Expansion — use «name» for Tab stops" }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 90
            TextArea {
                id: templateField
                objectName: "snippetTemplateField"
                wrapMode: TextEdit.WrapAnywhere
                placeholderText: "\\operatorname{span}\\{«v_1», \\ldots, «v_n»\\}"
                font.family: "monospace"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Switch { id: enabledSwitch; text: "Enabled"; checked: true }
            Switch {
                id: allCoursesSwitch
                objectName: "snippetAllCoursesSwitch"
                text: "All courses"
                checked: true
                onToggled: {
                    if (!checked && !coursesField.text.length && manager.currentCourse.length)
                        coursesField.text = manager.currentCourse
                }
            }
            Item { Layout.fillWidth: true }
        }

        TextField {
            id: coursesField
            objectName: "snippetCoursesField"
            Layout.fillWidth: true
            visible: !allCoursesSwitch.checked
            placeholderText: "Analys 1, Linjär algebra"
        }

        Label {
            Layout.fillWidth: true
            visible: manager.hasBuiltInConflict()
            text: "This trigger or alias overrides a built-in snippet in the selected scope."
            color: "#d49b32"
            wrapMode: Text.Wrap
        }
        Label {
            id: statusLabel
            objectName: "snippetStatusLabel"
            Layout.fillWidth: true
            visible: manager.statusText.length > 0
            text: manager.statusText
            color: manager.statusText === "Saved" || manager.statusText === "Deleted"
                   ? palette.highlight : "#d65c5c"
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Save snippet"; onClicked: manager.saveCurrent() }
            Button { text: "Close"; onClicked: manager.close() }
        }
    }

    Dialog {
        id: deleteConfirm
        parent: manager.parent
        anchors.centerIn: parent
        modal: true
        title: "Delete snippet?"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: manager.deleteCurrent()
        Label { text: "This removes the selected custom snippet." }
    }

    Dialog {
        id: importConfirm
        parent: manager.parent
        anchors.centerIn: parent
        modal: true
        title: "Replace custom snippets?"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: importDialog.open()
        Label {
            width: 360
            wrapMode: Text.Wrap
            text: "Import replaces the current custom snippet list. Export it first if you want a backup."
        }
    }

    Dialogs.FileDialog {
        id: importDialog
        title: "Import FoldTeX snippets"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["FoldTeX snippets (*.foldtex-snippets.json)", "JSON files (*.json)"]
        onAccepted: {
            var result = manager.backendApi.importCustomSnippets(selectedFile.toString())
            manager.statusText = result.saved ? "Imported" : (result.error || "Import failed")
            if (result.saved) manager.reload()
        }
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: "Export FoldTeX snippets"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["FoldTeX snippets (*.foldtex-snippets.json)"]
        defaultSuffix: "foldtex-snippets.json"
        onAccepted: {
            var result = manager.backendApi.exportCustomSnippets(selectedFile.toString())
            manager.statusText = result.saved ? "Exported" : (result.error || "Export failed")
        }
    }
}
