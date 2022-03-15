# Paramiko `write_private_key_file` race condition

[Paramiko](https://www.paramiko.org) is a pure Python SSH library. Among other uses, some cloud SDKs/tools (notably also the Azure CLI) use it to generate SSH keys.

## Vulnerability

Paramiko's [\_write_private_key_file](https://github.com/paramiko/paramiko/blob/2.10.0/paramiko/pkey.py#L561) up to and including version 2.10.0 first opens (creates) the file, then `chmod`s it, then writes the key into it. While this may appear secure, it isn't: If an attacker manages to open the file in the brief moment between its creation and the chmod, the file handle remains valid even after the chmod, and the attacker can read the key once it is written.

To demonstrate this, run `./raceread /home/victim/paramiko-demo.key` while running a [simple key generation example](paramiko_poc.py) under the victim's account.

To exploit this vulnerability, the attacker has to have the ability to open the file, i.e. this is typically a **local-only** vulnerability. I've confirmed that this was reliably exploitable (if the file is being created in a directory that the attacker can read). Due to questionable choices by some distributions, many home directories are created with permissions 755, i.e. world-readable. (The `.ssh` directory typically isn't created as world-readable, but this cannot be guaranteed and users of the library may write keys anywhere, including e.g. `/tmp`).

The vulnerability is tracked as [**CVE-2022-24302**](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-24302). ([Changelog entry](https://www.paramiko.org/changelog.html#2.10.1))

## Fixed in

Version [2.10.1](https://www.paramiko.org/changelog.html#2.10.1) fixes the issue.

Thanks to Jeff Forcier (the maintainer of Paramiko) for his quick response and for quickly, patiently and thoroughly working together to find the best solution!

## Solving the problem in Python

Python has two `open` functions: The high level `open()` call (aka `io.open`) which returns a file object, and the low level `os.open` which returns a file descriptor. The latter accepts both flags and a `mode` value which is used to set the file mode (permissions) on Linux/POSIX if the `open()` call ends up creating a new file.

A regular `open('somefile', 'w')` call will be [translated](https://github.com/python/cpython/blob/47cca0492b3c379823d4bdb600be56a633e5bb88/Lib/_pyio.py#L1543) into an `os.open` call with flags `os.O_CREAT | os.O_TRUNC | os.O_WRONLY` (plus potentially some OS-specific flags) and the specified mode. While the mode is mostly meaningless on Windows, it seems like the "user writeable" bit is used to determine the file's readonly attribute, and other than that, it is ignored. This *should* make it safe to use the same `os.open` call across all common platforms. The file descriptor can then either be wrapped into a file object using e.g. `os.fdopen`, or closed - once the file has been created with the correct permissions, it can then be reopened safely.

## Timeline

* **2022-01-24** Initial report.
* **2022-01-24** Tidelift security contact confirms receipt (human response) and later connects me with the maintainer.
* (some discussion about the best possible fix, during which I learned quite a bit about the details of `open()` and had to correct my own wrong assumptions several times)
* **2022-03-12** Fix released.
