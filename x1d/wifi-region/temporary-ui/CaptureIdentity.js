.pragma library

// Accept the capture event's explicit path only. Do not inspect the last row
// of a catalogue, guess a file number, or use a previous capture as fallback.
function pathFromEvent(value) {
    var text=String(value || "")
    var prefix=/^image:\/\/imagestore\/(?:preview|thumb|fullsize)/
    if(prefix.test(text))text=text.replace(prefix,"")
    if(text.charAt(0)!=="/" || text.indexOf("\\")>=0 || text.indexOf("\u0000")>=0 || text.indexOf("//")>=0)return ""
    var parts=text.split("/")
    for(var i=0;i<parts.length;i++)if(parts[i]==="." || parts[i]==="..")return ""
    return /\.(3fr|jpg)$/i.test(text)?text:""
}
function providerSource(value) {
    var path=pathFromEvent(value)
    return path?"image://hbljpeg/"+encodeURIComponent(path):""
}
