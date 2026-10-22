// 离线候选：左右眼沿用原厂字段身份。尚未连接实际 AF 控制接口。
function create(preferred, faceId) {
    return {preferred:preferred, faceId:faceId, chosen:preferred,
            missingSince:-1, preferredSince:-1, lastTime:-1};
}
function nearestEye(left, right, x, y, width, height) {
    var eyes=[left,right], chosen=-1, best=Infinity;
    for(var i=0;i<2;++i) {
        var r=eyes[i];
        if(!r || !isFinite(r.x) || !isFinite(r.y) || !isFinite(r.width) ||
                !isFinite(r.height) || r.width<=0 || r.height<=0 ||
                r.x>=1 || r.y>=1 || r.x+r.width<=0 || r.y+r.height<=0) continue;
        var dx=(r.x+r.width/2)*width-x, dy=(r.y+r.height/2)*height-y;
        var distance=dx*dx+dy*dy;
        if(distance<best) {best=distance;chosen=i;}
    }
    return chosen;
}
function step(s, preferred, faceId, leftValid, rightValid, now, delay) {
    if(s.preferred!==preferred || s.faceId!==faceId || now<s.lastTime)
        s=create(preferred,faceId);
    s.lastTime=now;
    var valid=[leftValid,rightValid];
    if(faceId<0) valid=[false,false];
    if(valid[s.chosen]) s.missingSince=-1;
    else if(s.missingSince<0) s.missingSince=now;
    if(s.chosen!==preferred && valid[preferred]) {
        if(s.preferredSince<0) s.preferredSince=now;
        if(now-s.preferredSince>=delay) {
            s.chosen=preferred; s.missingSince=-1; s.preferredSince=-1;
        }
    } else s.preferredSince=-1;
    if(!valid[s.chosen] && s.missingSince>=0 && now-s.missingSince>=delay && valid[1-s.chosen]) {
        s.chosen=1-s.chosen; s.missingSince=-1; s.preferredSince=-1;
    }
    // 等待期间不使用已丢失的旧眼部坐标，也不伪画上一帧的眼框。
    var eye=valid[s.chosen] ? s.chosen : -1;
    return {state:s, eye:eye, fallbackFace:faceId>=0 && eye<0 &&
            s.missingSince>=0 && now-s.missingSince>=delay};
}
