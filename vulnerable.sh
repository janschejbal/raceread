#!/bin/bash
SECRET_FILE='/tmp/demo-vulnerable-secret'
SECRET=$(tr -cd '0-9a-f' < /dev/urandom | head -c 32)

touch "$SECRET_FILE"
chmod 600 "$SECRET_FILE"  # This is unsafe (see README.md)
echo "simulated_secret_that_should_only_be_available_to_root=$SECRET" >> "$SECRET_FILE"
sleep 1
rm "$SECRET_FILE"
