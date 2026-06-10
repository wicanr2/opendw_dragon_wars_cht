cd /app
rm -rf /tmp/obj; mkdir -p /tmp/obj /out
for f in src/lib/*.c; do gcc -c -O1 -w -Isrc/lib "$f" -o "/tmp/obj/$(basename ${f%.c}).o" 2>/dev/null; done
ar rcs /tmp/obj/libdragon.a /tmp/obj/*.o 2>/dev/null
for spec in "resextract:src/tools/resextract.cpp" "monster_info:src/tools/monster_info.cpp" "sprite_dump:src/tools/sprite_dump.cpp"; do
  t=${spec%%:*}; src=${spec#*:}
  if g++ -O1 -w -std=c++14 -Isrc/lib "$src" /tmp/obj/libdragon.a -o /out/$t 2>/tmp/obj/$t.err; then echo "OK  $t"; else echo "FAIL $t"; grep -E 'error|undefined' /tmp/obj/$t.err|sort -u|head -6; fi
done
