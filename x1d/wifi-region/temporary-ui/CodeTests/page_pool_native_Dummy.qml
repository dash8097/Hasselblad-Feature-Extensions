import QtQuick 2.5
Item {
    property var probe
    property string token
    property int serial:0
    property int prepared:0
    property int deactivated:0
    property int activated:0
    property string initial:''
    property string initialInput:'original'
    property bool failPrepare:false
    property bool failDeactivate:false
    property bool failActivate:false
    function residentPrepare(){prepared++;probe.record('prepare',token);probe.callback('prepare',token);if(failPrepare)throw new Error('fixture prepare failure')}
    function residentDeactivate(){deactivated++;probe.record('deactivate',token);probe.callback('deactivate',token);if(failDeactivate)throw new Error('fixture deactivate failure')}
    function residentActivate(){activated++;probe.record('activate',token);if(failActivate)throw new Error('fixture activate failure')}
    function originalExit(){closeMenu()}
    function originalControl(){closeMainScreen()}
    Component.onCompleted:{serial=++probe.created;initial=initialInput;probe.record('construct',token)}
}
