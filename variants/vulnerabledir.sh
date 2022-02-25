#!/bin/bash
set -eu
SECRET_DIR='/tmp/demo-vulnerable-dir/'
SECRET_FILE='demo-vulnerable-secret'
SECRET=$(tr -cd '0-9a-f' < /dev/urandom | head -c 32)

mkdir -p "$SECRET_DIR/subdir"
# The attacker can now grab handles to $SECRET_DIR and the subdirectory
chmod 700 "$SECRET_DIR"  # This is insufficient (see README.md)

# The attacker will be able to read this file using the subdirectory handle.
# The permissions on subdir are unchanged and parent permissions don't matter.
touch "$SECRET_DIR/subdir/$SECRET_FILE"
chmod 600 "$SECRET_DIR/subdir/$SECRET_FILE"  # This is unsafe (see README.md)
echo "simulated_secret_that_should_only_be_available_to_root=$SECRET" >> "$SECRET_DIR/subdir/$SECRET_FILE"

# However, the attacker will NOT be able to read this file on systems that
# do not support O_SEARCH. Even though the attacker has the handle to
# $SECRET_DIR open, the permissions of the file's parent directory are checked
# during the openat() call, and the attacker does not have search (aka
# traverse, eXecute) permission on the dir anymore.
# With a properly implemented O_SEARCH, this check must NOT be performed, and
# the attacker WILL be able to read the file!
touch "$SECRET_DIR/$SECRET_FILE"
chmod 600 "$SECRET_DIR/$SECRET_FILE"  # Still unsafe (see README.md)
echo "simulated_secret_that_should_only_be_available_to_root=$SECRET" >> "$SECRET_DIR/$SECRET_FILE"

sleep 1

rm "$SECRET_DIR/subdir/$SECRET_FILE"
rm "$SECRET_DIR/$SECRET_FILE"
rmdir "$SECRET_DIR/subdir"
rmdir "$SECRET_DIR"
