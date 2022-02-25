# Backblaze B2 cli and Python SDK

Backblaze B2 is a cloud (IaaS) storage service, similar to Amazon S3 or Google Cloud Storage. The B2 command line tool and SDK are used to interact with this service. To do so, the tool and SDK need to store authentication credentials. 


## Vulnerability

An issue with file permissions allows unprivileged users on a shared Linux machine to steal credentials from other unprivileged users on that machine if they use a vulnerable version of the command line tool or SDK.

Preconditions:

* home directories are world-readable (this is e.g. the default on Ubuntu)
* the attacker is active when the victim runs `b2-linux authorize-account` for the first time

This is possible because the [database is first created, then `chmod`ed, exposing the (empty) database for a brief moment](https://github.com/Backblaze/b2-sdk-python/blob/afbe05a244d6660c0d97e01b339118697572da7e/b2sdk/account_info/sqlite_account_info.py#L166).

If an attacker opens a file handle after the file is created, but before it is `chmod`ed, the file handle remains valid and allows the attacker to read the contents of the file later.

This is a **local** attack with a limited window of opportunity: It can only be exploited if the attacker already has the ability to read files on the system when the victim is creating the credential file.


## Proof of concept

Prepare the attack by building and starting the attack tool (this has to be dome *before* `/home/victim/.b2_account_info` is created, i.e. before the user runs the `b2` tool for the first time): 

```
attacker@...$ gcc -o raceread raceread.c && ./raceread /home/victim/.b2_account_info
```

Victim logs in with `./b2-linux authorize-account`, creating the file... and raceread steals it. 

Extract the credentials from the dumped file:

```
attacker@...$ sqlite3 -column -header raceread.out "select account_id, account_id_or_app_key_id, application_key, account_auth_token from account"
```

I have only tested this on a minimal installation of Ubuntu 20.04 where credentials are written to `~/.b2_account_info`. The attack should work just as well if `XDG_CONFIG_HOME` is used, assuming that directory is world-readable. (At least the graphical Ubuntu installation I looked at did not set that variable, and `~/.config/` was world-readable too).


## Fixed in

[v3.2.1 of the command line tool](https://github.com/Backblaze/B2_Command_Line_Tool/commit/c74029f9f75065e8f7e3c3ec8e0a23fb8204feeb) and [v1.14.1 of the SDK](https://github.com/Backblaze/b2-sdk-python/blob/master/CHANGELOG.md#1141---2022-02-23) resolve this issue.


## Advisories

* [B2 Command Line Tool TOCTOU application key disclosure](https://github.com/Backblaze/B2_Command_Line_Tool/security/advisories/GHSA-8wr4-2wm6-w3pr) ([CVE-2022-23651](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23653))
* [b2-sdk-python TOCTOU application key disclosure](https://github.com/Backblaze/b2-sdk-python/security/advisories/GHSA-p867-fxfr-ph2w) ([CVE-2022-23651](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2022-23651))


## Timeline

* **2022-01-16** Initial report
* (some communication about logistics, credit and details of the vulnerability)
* **2022-02-23** Backblaze releases the fixed versions and advisories, crediting me for the report, but states that a bug bounty can only be paid for issues reported via the Bugcrowd platform.
