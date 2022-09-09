# Microsoft Azure CLI leaves config world-readable

While looking for software vulerable to the race condition described in the [README](../README.md), I noticed that Microsoft's Azure CLI tool doesn't even attempt to change the permissions, storing credentials in a world-readable file in a world-readable directory (`~/.azure/msal_token_cache.json`).

## Affected version

Tested on the following version:

```
$ az version
{
  "azure-cli": "2.32.0",
  "azure-cli-core": "2.32.0",
  "azure-cli-telemetry": "1.0.6",
  "extensions": {}
}
```

Since the `FilePersistence` module of MSAL (used to store the credentials) does not adjust file permissions, other software using it may be vulnerable in a same way if it doesn't adjust the permissions and doesn't use a protected directory. It may also be vulnerable to the race condition describd in the README if it adjusts the permissions only after the file is created.

## Location of vulnerability

The Azure CLI uses the [Microsoft Authentication Library (MSAL)](https://docs.microsoft.com/en-us/azure/active-directory/develop/msal-overview) under the hood. While that library supports encrypting tokens with Libsecret (which would mitigate the impact of this issue), this is not used. 

The following files are located in `~/lib/azure-cli/lib/python3.8/site-packages/`.

`azure/cli/core/auth/identity.py` has an `encrypt` parameter defaulting to False, which is passed to `build_persistence` in `azure/cli/core/auth/persistence.py` where it influences the choice between encrypted storage methods (`LibsecretPersistence` on Linux) and the unencrypted `FilePersistence`. This is called from `azure/cli/core/_profile.py`, where encryption is enabled only on Windows by default.

The `LibsecretPersistence` code in `azure/cli/core/auth/persistence.py` seems unfinished, containing placeholder variable names.

A possible reason for encryption being disabled by default on non-Windows operating systems could be that libsecret doesn't work in many environments where the Azure CLI may be used (e.g. a SSH session to a headless server with a minimal install).

## Report

*Note: Formatting was adjusted for Markdown.*

Hi,  
it seems that the Azure CLI stores access tokens in a world-readable file in a world-readable/world-traversable directory on Linux (at least on systems that don't have a GUI installed). I tested this by running "az login" on a Scaleway VM with Ubuntu 20.04.3 LTS.

```
victim@scw-angry-chandrasekhar:~$ ll ~/.azure
total 68
drwxrwxr-x 5 victim victim 4096 Jan  9 02:18 ./
drwxr-xr-x 8 victim victim 4096 Jan  9 02:17 ../
[...]
-rw-rw-r-- 1 victim victim 7363 Jan  9 02:18 msal_token_cache.json
[...]
```

Since some distributions (notably e.g. Ubuntu 20.04 LTS) create home directories as world-readable/world-traversable (permissions = 755) by default, this means that any other user of a system can read tokens by simply running `cat /home/victim/.azure/msal_token_cache.json`


Example (note the different user):

```
attacker@scw-angry-chandrasekhar:~$ cat /home/victim/.azure/msal_token_cache.json
{
    "AccessToken": {
        "0000[...]": {
            "credential_type": "AccessToken",
            "secret": "eyJ0[...]
```

I have not tried actually signing in or taking action with these tokens, since I used a throwaway Microsoft account that don't have an Azure account, but they do look like valid JWTs to me.

It seems like the Azure CLI uses MSAL, so this may affect everything that uses MSAL in a similar way. It is possible that on a system with a graphical install, MSAL would use libsecret and thus wouldn't be affected.

Please let me know if you are able to reproduce this issue and whether you agree that this is a security issue.

Cheers,  
Jan

## Timeline

(Dates without a time may be off-by-one depending on time zone)

* **2022-01-09** Initial report (and automatic response).
* **2022-01-10** MSRC confirms that a case has been opened.
* **2022-01-16** I follow up, asking whether they were able to confirm the issue.
* **2022-01-26** I follow up, asking whether they were able to confirm the issue, and announce my intent to follow a disclosure policy [similar to Project Zero](https://googleprojectzero.blogspot.com/2021/04/policy-and-disclosure-2021-edition.html).
* **2022-01-30** A [commit](https://github.com/AzureAD/microsoft-authentication-extensions-for-python/commit/48bc3d71ecd0fd7e16bb0d9a915b44999cda2cd4) that seems to address the issue is submitted to the MSAL authentication extensions repository. (I only discover this on 2022-03-14.)
* **2022-02-01** MSRC sends me a survey request.
* **2022-02-03** MSRC sends me a newsletter.
* **2022-02-15** [msal-extensions 1.0.0](https://pypi.org/project/msal-extensions/1.0.0/#files) is released, likely fixing the issue where the new version is used. (I only discover this on 2022-03-14.)
* **2022-02-16** MSRC sends me a survey request.
* **2022-02-24** I follow up, asking whether they were able to confirm the issue, and reiterate my intent to follow a disclosure policy [similar to Project Zero](https://googleprojectzero.blogspot.com/2021/04/policy-and-disclosure-2021-edition.html).
* **2022-02-28T19:36:12Z** MSRC confirms that "this report does not meet our bar for servicing in a security update" and is ok to publish.
* **2022-02-28T19:36:22Z** MSRC sends another copy of that e-mail.
* **2022-02-28T19:37:22Z** MSRC informs me: "We confirmed the behavior you reported. We'll continue our investigation and determine how to address this issue."
* **2022-02-28T20:26:57Z** I tell MSRC that I'm going to publish, and to let me know ASAP if the first two mails were a mistake.
* **2022-03-02** MSRC sends me a survey request.
* **2022-03-02** Write-up published.
* **2022-03-14** I discover the 2022-01-30 commit and 2022-02-15 release. However, a new install of the CLI still installs the old version of msal-extensions (likely because [azure-cli-core/setup.py limits the version](https://github.com/Azure/azure-cli/blob/88846cd205257cd05fd4f2f3a0b28b72511de6f7/src/azure-cli-core/setup.py#L54)).
* **2022-07-15** I verify that this is fixed, even using an old copy of `az` that was likely installed on May 3. I believe this may have been fixed in 2.36.0, released 2022-04-26.
