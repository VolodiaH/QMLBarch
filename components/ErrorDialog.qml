import QtQuick 2.15
import QtQuick.Controls 2.15

Dialog {
    id: dlg
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    closePolicy: Popup.NoAutoClose
    focus: true


    title: "Error"
    standardButtons: Dialog.Ok
    implicitWidth: 320
    implicitHeight: 160

    property alias message: msg.text

    contentItem: Column {
        spacing: 12
        padding: 16
        Label { id: msg; text: "" }
    }
}
