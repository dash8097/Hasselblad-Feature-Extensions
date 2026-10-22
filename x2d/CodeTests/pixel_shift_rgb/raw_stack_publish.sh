#!/system/bin/sh
set -eu
test "$#" = 2
s=$1; format=$2
echo "${s##*/}" | grep -Eq '^native-album-ui-[0-9a-f]{8}-[1-9][0-9]{0,9}$'
test "${s%/*}" = /mnt/media_rw/ssd/multishot
test -f "$s/restored"; test -f "$s/raw.ready"; test -s "$s/merged.3FR"; test ! -L "$s/merged.3FR"
case "$format" in 3|4) ext=HEIC; source=album.heic;; 0|1|2) ext=JPG; source=album.jpg;; *) exit 2;; esac
test -s "$s/$source"; test ! -L "$s/$source"
album=$(cat "$s/output-plan")
echo "$album" | grep -Eq '^[1-9][0-9]{2}HASBL/[A-Z][0-9]{7}\.JPG$'
folder=${album%/*}; image=${album#*/}; image=${image%.JPG}; d="/mnt/media_rw/ssd/DCIM/$folder"
if [ ! -e "$d" ] && [ ! -L "$d" ]; then mkdir "$d"; fi
test -d "$d"; test ! -L "$d"
for ending in JPG HEIC 3FR raw-partial stack-partial; do test ! -e "$d/$image.$ending"; test ! -L "$d/$image.$ending"; done
test "$(stat -c %d "$s/merged.3FR")" = "$(stat -c %d "$d")"
test "$(stat -c %d "$s/$source")" = "$(stat -c %d "$d")"
raw_moved=0; compressed_moved=0; raw_renamed=0; compressed_renamed=0; committed=0
move_owned(){
 test ! -e "$2"; test ! -L "$2"
 mv -n "$1" "$2"
 # mv -n 遇到并发出现的目的文件可返回成功；源文件消失才表示交接成功。
 test ! -e "$1"; test -s "$2"; test ! -L "$2"
}
finish(){
 rc=$?; trap - EXIT
 if [ "$committed" = 0 ]; then
  if [ "$compressed_moved" = 1 ]; then
   test ! -e "$s/$source"
   if [ "$compressed_renamed" = 1 ]; then move_owned "$d/$image.$ext" "$s/$source"; else move_owned "$d/$image.stack-partial" "$s/$source"; fi
  fi
  if [ "$raw_moved" = 1 ]; then
   test ! -e "$s/merged.3FR"
   if [ "$raw_renamed" = 1 ]; then move_owned "$d/$image.3FR" "$s/merged.3FR"; else move_owned "$d/$image.raw-partial" "$s/merged.3FR"; fi
  fi
 fi
 exit "$rc"
}
trap finish EXIT
move_owned "$s/merged.3FR" "$d/$image.raw-partial"; raw_moved=1
move_owned "$s/$source" "$d/$image.stack-partial"; compressed_moved=1
test -s "$d/$image.raw-partial"; test -s "$d/$image.stack-partial"
move_owned "$d/$image.raw-partial" "$d/$image.3FR"; raw_renamed=1
move_owned "$d/$image.stack-partial" "$d/$image.$ext"; compressed_renamed=1
sync; committed=1
# 登记失败保留完整成片和诊断，不能误报拍摄成功。
/bin/dbus-send --system --print-reply --reply-timeout=5000 --dest=com.hasselblad.storage /storage com.hasselblad.storage.browse "string:/ssd/$folder" int32:0 > "$s/album-browse.log"
grep -q "string \"$image\"" "$s/album-browse.log"
(set -C; printf '%s/%s.%s\n' "$folder" "$image" "$ext" > "$s/album-location")
