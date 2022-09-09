# Python shutil.copy2 race condition

shutil.copy2 first copies the contents (creating the file with default permissions), then sets the permissions. This allows a local attacker that can read the directory to quickly grab the contents of the file.

Source code: https://github.com/python/cpython/blob/3.10/Lib/shutil.py#L434

Note that changing the permissions after file creation but before writing content is insufficient: if the file is created with permissions that allow the attacker to open it, the attacker can get a file handle for the (empty) file. At least on Linux, the handle remains valid and readable even if the permissions change, allowing the attacker to read the file contents when they're populated.

I've confirmed that this is reliably exploitable (if the file is being created in a directory that the attacker can read). Due to questionable choices by some distributions, many home directories are created with permissions 755, i.e. world-readable. Specifically, I've tested this on Python 3.8.10 on Ubuntu 20.04.3 LTS:

```
victim@testhost:~$ bash -c 'umask 077; rm -f python-file1 python-file2; touch python-file1; cat /dev/urandom | tr -cd a-f0-9 | head -c 20 >> python-file1'
victim@testhost:~$ ls -lah python-file*
-rw------- 1 victim victim 20 Feb  4 20:27 python-file1
victim@testhost:~$ python3 -c 'import shutil; shutil.copy2("python-file1", "python-file2")'
victim@testhost:~$ ls -lah python-file*
-rw------- 1 victim victim 20 Feb  4 20:27 python-file1
-rw------- 1 victim victim 20 Feb  4 20:27 python-file2
```

Meanwhile (raceread has to be started before the above commands are run):

```
attacker@testhost:~$ ./raceread /home/victim/python-file2
Attempting to repeatedly open '/home/victim/python-file2' until successful...
No such file or directory
File is open!


===== Stat change =====
mode: o664   uid: 1001   gid: 1001   size: 0
atime: 1644006471   mtime: 1644006471   ctime: 1644006471
-- content --
29757a922b790095434a-- end of content (20 bytes) --
-- wrote copy into raceread.out --

```

The `cp` command does not suffer from this:

```
attacker@testhost:~$ ./raceread /home/victim/cp-file2
Attempting to repeatedly open '/home/victim/cp-file2' until successful...
No such file or directory
Permission denied
```

## Timeline

* **2022-02-04** Initial report (and autoresponse).
* **2022-02-05** Manual response, confirming the issue.
* **2022-03-02** I followed up, asking whether there are any new developments, and stated that I intend to follow a 90-day disclosure timeline, similar to Project Zero (no response received).
* **2022-09-09** I follow up, announcing that I'll be disclosing soon, and am asked to create an Issue on Github.
* **2022-09-09** Public disclosure, [public issue](https://github.com/python/cpython/issues/96719) filed
