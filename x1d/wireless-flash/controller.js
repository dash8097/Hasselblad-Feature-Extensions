function WirelessFlashController(sendOnce) {
    "use strict";
    var enabled = false, source = 0, delayMs = 0;
    var generation = 0, lastShot = -1, shot = null, pending = null;
    var lastTime = -1, lastError = "";
    function integer(n, lo, hi) {
        return typeof n === "number" && isFinite(n) &&
               Math.floor(n) === n && n >= lo && n <= hi;
    }
    function time(t) {
        if (!integer(t, 0, 9007199254740991) || t < lastTime) {
            cancel("invalid_clock"); return false;
        }
        lastTime = t; return true;
    }
    function cancel(reason) {
        generation++; pending = null; shot = null;
        lastError = reason || "";
    }
    function configure(on, newSource, ms) {
        if (typeof on !== "boolean" || !integer(newSource, 0, 3) ||
            !integer(ms, 0, 5000)) {
            enabled = false; cancel("invalid_configuration"); return false;
        }
        cancel("");
        enabled = on; source = newSource; delayMs = ms;
        return true;
    }
    function phase(id, point, now) {
        if (!time(now) || !shot || id !== shot.id ||
            !integer(point, 0, 3) || point <= shot.lastPhase) return false;
        shot.lastPhase = point;
        if (!enabled || point !== shot.source || shot.consumed) return false;
        shot.consumed = true;
        pending = { id: id, source: point, generation: generation,
                    due: now + shot.delayMs };
        return true;
    }
    function begin(id, electronic, now) {
        if (!time(now) || !integer(id, 0, 2147483647) ||
            typeof electronic !== "boolean") return false;
        if (id <= lastShot) return false;
        cancel(""); lastShot = id;
        if (!enabled || !electronic) return false;
        shot = {id: id, source: source, delayMs: delayMs,
                lastPhase: -1, consumed: false};
        phase(id, 0, now); return true;
    }
    function tick(now) {
        if (!time(now) || !pending || now < pending.due) return false;
        var p = pending; pending = null;
        if (!enabled || !shot || shot.id !== p.id ||
            p.generation !== generation) return false;
        // Do not replay a request after the controller stalls.
        if (now - p.due > 250) { cancel("deadline_missed"); return false; }
        try { sendOnce({shot: p.id, source: p.source, generation: p.generation}); }
        catch (e) { cancel("send_failed"); return false; }
        return true;
    }
    function end(id) {
        if (shot && shot.id === id) { cancel(""); return true; }
        return false;
    }
    return {configure: configure, begin: begin, phase: phase, tick: tick,
            end: end, cancel: cancel,
            inspect: function() {
                return {enabled:enabled, source:source, delayMs:delayMs,
                        pending:pending !== null, error:lastError};
            }};
}

