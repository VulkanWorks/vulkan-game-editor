import QtQuick.Controls 2.15
import QtQuick 2.15

Rectangle {
  id : itemContainer

  readonly property int fixedWidth : 36 * 4
  readonly property int minHeight : 36
  readonly property int maxHeight : itemContainerView.cellHeight * Math.ceil(model.rowCount() / 4)
  property variant model
  property int preferredHeight : maxHeight

  width : fixedWidth

  onHeightChanged : {
    console.log("height: " + height);
  }

  color : "#777"
  Component {
    id : itemDelegate

    Rectangle {
      required property int serverId

      width : itemContainerView.cellWidth
      height : itemContainerView.cellHeight

      color : "transparent"

      Rectangle {
        width : itemContainerView.cellWidth - 4
        height : itemContainerView.cellHeight - 4

        anchors.centerIn : parent
        color : "#333"

        Image {
          anchors.fill : parent
          source : {
            console.log(serverId);
            return serverId != -1 ? "image://itemTypes/" + serverId : "";
          }
        }
      }
    }
  } // Component: itemDelegate

  GridView {
    id : itemContainerView
    model : parent.model

    anchors.centerIn : parent


    width : parent.width
    height : Math.min(parent.height, cellHeight * Math.ceil(model.rowCount() / 4))
    cellWidth : 36
    cellHeight : 36
    contentHeight : 36 * Math.ceil(model.rowCount() / 4)
    clip : true
    // boundsBehavior : Flickable.StopAtBounds
    boundsBehavior : Flickable.OvershootBounds
    flow : GridView.LeftToRight
    snapMode : ListView.NoSnap

    delegate : itemDelegate
    focus : true

    ScrollBar.vertical : ScrollBar {
      visible : itemContainerView.height < itemContainerView.contentHeight
      policy : ScrollBar.AlwaysOn
      parent : itemContainerView.parent
      anchors.top : itemContainerView.top
      anchors.left : itemContainerView.right
      anchors.bottom : itemContainerView.bottom

      contentItem : Rectangle {
        implicitWidth : 10
        implicitHeight : 64
        radius : 0
        color : parent.pressed ? "#606060" : "#cdcdcd"

        Behavior on color {
          ColorAnimation {
            duration : 250
          }
        }
      }
    }
    // Scrollbar

    // Vertical resize
    MouseArea {
      property int oldMouseY
      cursorShape : containsMouse || pressed ? Qt.SizeVerCursor : Qt.ArrowCursor
      preventStealing : true

      anchors.left : parent.left
      anchors.right : parent.right
      y : parent.y + parent.height - 5 / 2
      width : parent.width
      height : 5
      hoverEnabled : true

      onPressed : {
        applicationContext.setCursor(Qt.SizeVerCursor);
        oldMouseY = mouseY;
      }
      onReleased : {
        applicationContext.resetCursor();
      }

      onPositionChanged : {
        if (pressed) {
          const newHeight = Math.max(itemContainer.minHeight, Math.min(itemContainer.height + (mouseY - oldMouseY), itemContainer.maxHeight));
          itemContainer.preferredHeight = newHeight;
        }
      }
    }

  } // GridView
} // Rectangle