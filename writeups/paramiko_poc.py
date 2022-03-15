#!/usr/bin/python3
import os
import sys
import paramiko
KEYFILE = "paramiko-demo.key"
if os.path.exists(KEYFILE):
  os.remove(KEYFILE)
print("Python " + sys.version)
print("Paramiko " + paramiko.__version__)
key = paramiko.ecdsakey.ECDSAKey.generate()
key.write_private_key_file(KEYFILE)
print("Key generated and written to " + KEYFILE)
