set -eu
[ "$(id -u)" = 0 ] && [ "$(cat /proc/self/attr/current)" = u:r:su:s0 ] || exit 31
state() { while read -r dev path fs options rest; do [ "$path" != /system ] || printf '%s %s %s\n' "$dev" "$fs" "$options"; done < /proc/mounts; }
original=$(state)
case "$original" in '/dev/block/mmcblk0p17 ext4 ro,'*) ;; *) exit 33;; esac
hashok() { actual=$(/system/bin/sha256sum "$1"); [ "${actual%% *}" = "$2" ]; }
restore() { case "$(state)" in '/dev/block/mmcblk0p17 ext4 rw,'*) /system/bin/mount -o remount,ro /system;; esac; }
# The normal maintenance domain verifies the original GUI before staging.
# This su-domain transaction only touches our system_file additions.
hashok /system/etc/x2d-preview-loader.sh abbce7a604008fe331ccc8d1ba872528022df4e1a70be226745c3736560e1085
[ ! -e /system/lib64/libx2d_menu_gate.so ] && [ ! -L /system/lib64/libx2d_menu_gate.so ] || exit 34
hashok /blackbox/x2d-menu-stage/libx2d_menu_gate.so b1d2f86e8a650187d370ff964bd6afcda2227b7ca3aff87dbc890d5a7aaf844b
[ ! -e /system/etc/x2d-menu-v1-main.qml ] && [ ! -L /system/etc/x2d-menu-v1-main.qml ] || exit 34
hashok /blackbox/x2d-menu-stage/x2d-menu-v1-main.qml 39ef3ee7b2e65db1776c769c03dc36333ced31c3e822ace90128a9ea901c7a0d
[ ! -e /system/etc/x2d-menu-candidate.sh ] && [ ! -L /system/etc/x2d-menu-candidate.sh ] || exit 34
hashok /blackbox/x2d-menu-stage/boot_menu_candidate.sh ee88ba07aeb7711a2af505a7bba6381468a9dbb8a9e96bf52e594257761b9f12
made=''
done_ok=0
finish() { if [ "$done_ok" = 0 ]; then for f in $made; do /system/bin/rm -f "$f"; done; fi; restore; }
trap finish EXIT
trap 'exit 43' HUP INT TERM
( /system/bin/sleep 10; restore ) </dev/null >/dev/null 2>&1 &
/system/bin/mount -o remount,rw /system
( set -C; : > /system/lib64/libx2d_menu_gate.so )
made="/system/lib64/libx2d_menu_gate.so $made"
/system/bin/cat /blackbox/x2d-menu-stage/libx2d_menu_gate.so > /system/lib64/libx2d_menu_gate.so
/system/bin/chmod 0644 /system/lib64/libx2d_menu_gate.so
hashok /system/lib64/libx2d_menu_gate.so b1d2f86e8a650187d370ff964bd6afcda2227b7ca3aff87dbc890d5a7aaf844b
( set -C; : > /system/etc/x2d-menu-v1-main.qml )
made="/system/etc/x2d-menu-v1-main.qml $made"
/system/bin/cat /blackbox/x2d-menu-stage/x2d-menu-v1-main.qml > /system/etc/x2d-menu-v1-main.qml
/system/bin/chmod 0644 /system/etc/x2d-menu-v1-main.qml
hashok /system/etc/x2d-menu-v1-main.qml 39ef3ee7b2e65db1776c769c03dc36333ced31c3e822ace90128a9ea901c7a0d
( set -C; : > /system/etc/x2d-menu-candidate.sh )
made="/system/etc/x2d-menu-candidate.sh $made"
/system/bin/cat /blackbox/x2d-menu-stage/boot_menu_candidate.sh > /system/etc/x2d-menu-candidate.sh
/system/bin/chmod 0644 /system/etc/x2d-menu-candidate.sh
hashok /system/etc/x2d-menu-candidate.sh ee88ba07aeb7711a2af505a7bba6381468a9dbb8a9e96bf52e594257761b9f12
/system/bin/sync
done_ok=1
restore
[ "$(state)" = "$original" ] || exit 44
echo MENU_CANDIDATE_INSTALLED_OLD_BOOT_UNCHANGED
