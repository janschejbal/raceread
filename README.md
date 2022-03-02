# chmod race condition proof of concept

Some scripts (and programs) create a file, change its permissions (mode), then fill it with secrets, like this:

```
touch "$SECRET_FILE"
chmod 600 "$SECRET_FILE"  # unsafe!
echo "$SECRET" >> "$SECRET_FILE"
```

Unfortunately, this is unsafe. If an attacker manages to open the file between the `touch` and the `chmod`, they can use the already-open file handle to read the contents that are inserted after the permission change.

The `raceread` program in this repo demonstrates this: It waits for a file to be created, opens it, then prints the contents any time the file stats change.

## Usage

Build the proof of concept with `make`, run it (`bin/raceread /tmp/demo-vulnerable-secret`), then in another window run e.g. `sudo ./vulnerable.sh`.

## Variants

Of course, there are many variants of this attack that this tool doesn't capture, e.g. where temporary files are created with an unpredictable name (in a listable directory, the attacker can find and open the file before the `chmod` happens).


### Directory permissions

A particularly insidious variant of this affects directory permissions. According to the [POSIX standard (aka the Open Group Base Specifications)](https://pubs.opengroup.org/onlinepubs/9699919799/functions/open.html), `openat()` can be used to open a file relative to a directory for which you have an already-open file descriptor. The standard also specifies what is supposed to happen regarding access checking (emphasis mine):

> If the access mode of the open file description associated with the file descriptor is not `O_SEARCH`, the function shall check whether directory searches are permitted using the **current** permissions of the directory underlying the file descriptor. **If the access mode is `O_SEARCH`, the function shall not perform the check.**

This means that if you can get an open file descriptor for the parent directory with access mode `O_SEARCH` (and the system implements the standard correctly), you can access the file even if the permissions of the directory change before the file is created! However, [Linux does not support `O_SEARCH`](https://stackoverflow.com/a/54893576) - _yet_ (as of January 2022). Since it may be implemented in the future, and since other systems likely behave differently, relying on it would be highly dangerous.

Even on systems where `O_SEARCH` is not supported, only changes to the immediate parent directory of the file matter: Once the attacker has a file handle for that directory, the directories above it do not matter anymore. Thus, if you place a file in `/tmp/secrets/subdir/secretfile`, once the attacker has a file handle for `subdir`, changes to the permissions of `secrets` are irrelevant.

TODO: Test on OSX!

### Related issues

If an attacker can e.g. pre-place an attacker controlled directory, symlink, hardlink, mount, pipe etc. where you want to put your file (either in the place of the file or one of its parents), a whole set of new issues arises that are not covered here.

## Solutions

* Many programming languages allow you to specify mode/permissions when creating the file.
  * If you can't control the place where the file is opened, you may be able to pre-create the file using the secure method first.
* For temporary files, use `mktemp` (or equivalent) which creates already locked down files and directories.
* `mkdir --mode` allows you to directly create a directory with a specific mode.
* The `umask` can be used, although this affects the entire process, making it risky to use in multithreaded applications and you need to restore the original umask afterwards.

### Home directory permissions

In many cases, these files are stored in the user's home directory. Unfortunately, some distributions (including Ubuntu before 21.04) default to creating home directories with world-readable/traversable (`755`) permissions. Ubuntu appears to have [finally changed that in 21.04](https://discourse.ubuntu.com/t/private-home-directories-for-ubuntu-21-04-onwards/19533) for newly created users (if `useradd` is used). To protect yourself, make sure your home directory is only readable by you: `chmod 700 ~`

## Vulnerable software

The following software was found to be vulnerable at some point in time:

* [Backblaze B2 cli and Python SDK](writeups/backblaze_b2.md)
* [Microsoft Azure CLI](writeups/microsoft_azure_cli.md) -- not this exact issue, file was simply left world-readable forever
* (to be continued as advisories are released)

## Credits

Shoutout to

* [Marten Seemann](https://github.com/marten-seemann) who pointed me to [Stéphane Chazelas' StackExchange answer](https://unix.stackexchange.com/a/180082) that points this out!
* [Jonathan Schleifer](https://github.com/Midar) for pointing me to the correct place to find the POSIX documentation and helping me understand some of the details of the issue.
