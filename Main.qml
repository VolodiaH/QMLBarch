import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: win
    width: 800
    height: 600
    visible: true
    title: "BARCH Compressor — " + startDir

    Component.onCompleted: {
            Qt.callLater(fileModel.refresh)
        }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: "Current Dir: " + startDir
                Layout.fillWidth: true
            }
            Button {
                text: "Refresh"
                onClicked: fileModel.refresh()
            }
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: fileModel
        delegate: Item {
            id: rowItem
            width: list.width
            height: 48

            Rectangle {
                anchors.fill: parent
                color: hasError
                       ? "#5d0000"
                       : (index % 2 ? "#0bb034" : "#139433")
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12
                leftPadding: 16
                rightPadding: 16

                Label { text: name; width: 420; elide: Text.ElideRight; color: hasError ? "#ffcccc" : "white" }
                Label { text: prettySize; width: 100; horizontalAlignment: Text.AlignRight; color: hasError ? "#ffcccc" : "#cccccc" }
                Label { text: statusText; width: 160; color: hasError ? "#ff8a8a" : (busy ? "#55c1ff" : "#a0a0a0") }
                BusyIndicator { running: busy; visible: busy; width: 24; height: 24 }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    console.log(index)
                    fileModel.process(index)
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Loader {
        id: errorLoader
        active: fileModel.hasError
        source: "components/ErrorDialog.qml"
        onLoaded: {

                errorLoader.item.parent = Overlay.overlay
                errorLoader.item.message = fileModel.errorText
                errorLoader.item.open()

                errorLoader.item.accepted.connect(() => {
                    fileModel.clearError()
                })
            }
    }
}
