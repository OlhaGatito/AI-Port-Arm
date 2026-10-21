awk "NR<=60 {printf \"%03d|%s|len=%s\n\", NR, \$0, length(\$0)}" /storage/roms/ports/ragingloop/assets/script/scenario01_start.txt
