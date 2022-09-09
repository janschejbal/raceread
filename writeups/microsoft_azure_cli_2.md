# Microsoft Azure CLI privilege escalation, possible credential storage race conditions

## Privilege escalation
A vulnerability in the Azure CLI (versions prior to 2.35.0) allows a local unprivileged user to execute code as another user running certain Azure CLI commands on Linux (at least when the Azure CLI was installed via the "curl|bash" method).

The azcopy installation routine [sets](https://github.com/Azure/azure-cli/blob/575ae2e6141d7021ae5fed4d7d512e5e5df631d7/src/azure-cli/azure/cli/command_modules/storage/azcopy/util.py#L57) the installation directory (~/bin/) as world-writeable as soon as the user first runs `az storage copy`.

This allows the attacker to e.g. simply replace the "az" binary with a malicious version:

```
mv -n /home/victim/bin/{az,az.bak}
echo -ne '#!/bin/bash\necho "IF YOU READ THIS YOU ($(whoami)) ARE PWNED"' > /home/victim/bin/az
chmod +x /home/victim/bin/az
```

As soon as the victim attempts to run "az" the next time, the attacker's code is executed (it's possible that an attacker can also execute code without another user interaction due to the shell completion code loaded from the .bashrc, I didn't look into that).

I have tested this on Ubuntu 20.04 LTS and azure-cli 2.34.1. This obviously only works on distributions that have permissive permissions on the home directories - recent Ubuntu versions have stopped doing that.

This issue can be seen in [this shell transcript](microsoft_azcopy_shell_victim_full.txt) or the [shortened, annotated version showing the full attack](microsoft_azcopy_shell_with_comments.txt).

### Affected versions
* Version 2.34.1 is vulnerable. (I have verified this.)
* Version 2.36.0 is no longer vulnerable. (I have verified this.)
* I believe that the [patch](https://github.com/Azure/azure-cli/pull/21714) was merged on 2022-03-29 and version 2.35.0 is the first version that is not vulnerable. (However, I have *not* verified this.)

### Advisory/acknowledgement
TODO: Add MS advisory/CVE number once provided

## Credential storage race conditions
I haven't tested this since this would require an Azure account, but I believe this issue allows a local unprivileged user to read certain Azure credentials of another user using the Azure CLI in versions prior to 2.35.0.

The function `_create_self_signed_cert` [writes](https://github.com/Azure/azure-cli/blob/575ae2e6141d7021ae5fed4d7d512e5e5df631d7/src/azure-cli/azure/cli/command_modules/role/custom.py#L1570) private key data into a file (`creds_file`) with default permissions before restricting access, instead of directly creating the file with appropriately restricted permissions. This allows an attacker to [open the file before permissions are restricted, and then read the contents](../README.md). Note that it is irrelevant whether the data is written before or after the permissions are restricted; as soon as the file is created with a permission setting that allows the attacker to open it, the attacker can obtain a file handle that will allow him to read the file later even if the permissions are changed.

Other parts of the function correctly used [`tempfile.mkstemp()`](https://docs.python.org/3/library/tempfile.html#tempfile.mkstemp), which is specifically designed to avoid this issue. However, for `creds_file`, `mkstemp()` is only used as a random name generator. (A random name does not protect against an attacker who can list the directory contents). 

I believe that the [patch](https://github.com/Azure/azure-cli/pull/21719) was merged on 2022-03-24 and version 2.35.0 is the first version that is not vulnerable. (However, I have *not* verified this.)

Additionally, the Azure CLI uses Paramiko and may thus also be affected by [CVE-2022-24302](../paramiko.md). I believe the new version of Paramiko was [added to the requirements.txt files on 2022-03-30](https://github.com/Azure/azure-cli/pull/21854), but it is possible that new installs were already using the fixed version since the version constraint in `setup.py` [allows](https://github.com/Azure/azure-cli/blob/48952e269bbcb51943dd0b9525e007f7238fde2c/src/azure-cli-core/setup.py#L57) this.

### Advisory/acknowledgement
TODO: Add MS advisory/CVE number once provided


## Timeline

All times and dates are given in the CET/CEST time zone ("Berlin time").

* **2022-03-15** I notify MS about CVE-2022-24302 in Paramiko (as a follow-up to my [previous report](microsoft_azure_cli.md)).
* **2022-03-15 01:58** I report "Issue 1" (`az storage copy` setting the install dir `~/bin` world-writeable) and "Issue 2" (race condition when writing `creds_file` in [`_create_self_signed_cert`](https://github.com/Azure/azure-cli/blob/575ae2e6141d7021ae5fed4d7d512e5e5df631d7/src/azure-cli/azure/cli/command_modules/role/custom.py#L1531)).
  * **2022-03-15 02:02** Autoresponse
  * **2022-03-16 22:17** Request for a video PoC
  * **2022-03-16 23:56** Case number is assigned
  * **2022-03-17 15:19** I refuse to make a video, but provide the two shell transcripts and offer a call to demonstrate.
* In the following weeks, Issue 1 is fixed. (I was not notified and only discovered this later.)
  * **2022-03-21** The pull requests to [fix Issue 1](https://github.com/Azure/azure-cli/pull/21714) and [fix Issue 2](https://github.com/Azure/azure-cli/pull/21719) are created.
  * **2022-03-29** The pull request for Issue 2 is merged into the default (`dev`) branch.
  * **2022-03-29** The pull request for Issue 1 is merged into the default (`dev`) branch.
  * **2022-03-30** Paramiko is [updated](https://github.com/Azure/azure-cli/pull/21854), addressing CVE-2022-24302.
  * **2022-04-05** Version 2.35.0 is released. I believe this is the first version that contains the fixes. 
* **2022-05-01 00:44** I ask for an update/confirmation regarding Issue 1, and reiterate the 90 day disclosure deadline.
  * **2022-05-02 04:28** I receive a response confirming that the fix is checked in and am asked to verify, share a copy of the draft of my post and delay publication until the end of the 90d deadline.
  * **2022-05-03 04:46** I confirm that Issue 1 is fixed, ask for confirmation that the fix happened in 2.35.0 and was released on April 5, and ask for a CVE number, reference or a page where I could link to a public acknowledgement. I note that I believe that Issue 2 is still not fixed (I was likely mistaken here).
  * **2022-05-03 23:09** I receive a promise to follow up.
* **2022-06-14** I receive a notification that the report is being reviewed for a possible bounty (and a simultaneous notification confirming the bug).
* **2022-07-11** I receive an e-mail about leaderboard acknowledgement.
  * several mails discussing the logistics (leaderboard, acknowledgements, later also bounty payments etc.) follow, with very fast human responses
* **2022-07-15** A 10k bounty is awarded! ([Corresponding to](https://www.microsoft.com/en-us/msrc/bounty-microsoft-azure?rtc=1) Security impact "Elevation of Privilege", Severity "Important", Report quality "High".)
* **2022-07-16** I discover that Issue 2 was likely fixed, and that Paramiko was updated.

## Sidenote: Overall handling of the bug bounty/vulnerability reporting program
First of all, thanks to the Microsoft team for the generous bounty awarded for the local privilege escalation issue! I consider the impact/severity assessment fair.

The responsiveness has improved significantly compared to my [previous report](microsoft_azure_cli.md), however I still found out about patch releases only when I actively asked. It is possible that I would have been able to see more updates had I registered for the MSRC Researcher Portal and reported there. The portal unfortunately provides a poor user experience: Multiple e-mail verifications, one of them buggy and possibly impossible to complete with Firefox configured with strict privacy settings, multiple prompts for the same data, my e-mail address suddenly appeared in a name field, and an overall unpolished appearance.

I'd wish for more proactive updates when patches are released. Until I discovered the patches while finalizing this write-up, I had the impression that the local information disclosure issues (allowing local users to access Azure credential of another user) weren't taken seriously, and I didn't follow up regarding Issue 2 given that the previously reported issue with the world-readable credential file was classified as not meeting the bar for a security update. Nevertheless, these issues were actually getting patched behind the scenes.

Given the generous bounty for the privilege escalation vulnerability, I still consider it worthwhile to participate in the Microsoft/Azure bug bounty program and to put in the effort to submit high-quality reports.
