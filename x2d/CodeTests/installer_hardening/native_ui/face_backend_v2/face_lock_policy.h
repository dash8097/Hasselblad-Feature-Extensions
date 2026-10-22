/* 本地扩展协议：普通目标编号不改变原厂行为；特殊命令仅由已确认后端的界面使用。 */
#define FACE_LOCK_COMMAND 0x40000000
#define FACE_UNLOCK_COMMAND 0x40010000
typedef struct { void *owner; int id; } FaceLock;
static int face_locked(const FaceLock *s, void *owner) { return s->owner==owner && s->id>=0; }
static int face_command(FaceLock *s, void *owner, int command, int *target) {
    if(command==FACE_UNLOCK_COMMAND) { s->owner=0; s->id=-1; return 2; }
    if(command>=FACE_LOCK_COMMAND && command<FACE_LOCK_COMMAND+65535) {
        s->owner=owner; s->id=command-FACE_LOCK_COMMAND; *target=s->id; return 1;
    }
    *target=command;
    return face_locked(s,owner) ? 3 : 0;
}
static int face_choose(const FaceLock *s, void *owner, int present, int fallback) {
    return face_locked(s,owner) && present ? s->id : fallback;
}
