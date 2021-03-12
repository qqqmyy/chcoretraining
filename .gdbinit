# Establish gdb connection through the port specified in build/gdb-port.
# Provide two commands: add-symbol-file-off and prepare-load-lib.
source ./scripts/gdb.py

add-symbol-file-off user/musl-1.1.24/build/lib/libc.so 0x400000000000
add-symbol-file-off build/kernel.img
