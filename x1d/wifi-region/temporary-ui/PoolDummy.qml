import QtQuick 2.5
Item {
    property bool activated:false
    function residentPrepare(){}
    function residentDeactivate(){activated=false}
    function residentActivate(){activated=true}
}
