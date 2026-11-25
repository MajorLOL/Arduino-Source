# Security of PocketPilot Studio

Looked at on 25 November 2026, by reading the source (not by watching the network of a running program). What follows is what that found, and what was done about it.

## What leaves the program

Nothing goes out when the program starts, and there is no telemetry, no analytics and no check for a new version. Everything below happens only when you use that part.

| What | Where to | When |
| --- | --- | --- |
| The conversation with Claude: your messages, pictures of the screen, text read from it, lines of the log and the options of the open program (cleaned, see below) | `api.anthropic.com` | Only with a Claude API key entered, and while you use Claude or Claude Plays |
| Source files and branches of this repository | `api.github.com`, `raw.githubusercontent.com` | Only with Claude; a commit only when you press **Commit** |
| The list of firmware, and a firmware file | GitHub, the repository `PokemonAutomation/Packages` | Only on the firmware step of the setup |
| The list of downloads, and the files (the OCR model) | `raw.githubusercontent.com` (Packages) | Only when you press the button to download in the settings, or start a program that needs a file that is not there (the zip has the OCR model) |
| The list of banned players and a stop switch of the Tera raid host | `raw.githubusercontent.com` (Pokémon Automation's `ServerConfigs-PA-SHA`) | Only when that program runs |
| Messages and pictures to a Discord channel | The webhook address that you enter | Only for the notifications that you switch on |

Error reports are not sent: the part that does it is in Pokémon Automation's official build only, and the setting is hidden. They are saved in `ErrorReportsLocal`, on your computer.

The program does not listen on any port. The Discord bot and the Discord Social SDK (a remote control, and a rich presence) are out of the Windows build.

## What is kept, and how

- **The Claude key and the GitHub token** are in `UserSettings/PocketPilot-Assistant-Keys.json`, encrypted with your Windows account. Claude never sees them.
- **Discord webhook addresses, the Discord token and the developer token** are in the settings file, sealed in the same way (`dpapi1:...`). A settings file from before is read as it is and sealed at the next save. A file copied to another account or computer cannot be opened: the secret has to be entered again.
- This protects against another Windows account and against a copied folder. It does not protect against a program that runs as you (see the next part).
- **Text that leaves the program** (what Claude reads from the log and the options, "Copy for Claude", the saved report) is cleaned: the name of the account in a path (`C:\Users\<user>\...`), Discord webhook addresses and bot tokens, Claude keys, GitHub tokens, and the value after `token`, `password`, `secret`, `api key` or `authorization`. It goes by the form of the text, so a line of the log that holds one is cleaned too.

## Other programs that run as you

Windows ties the encryption to your account, not to PocketPilot. A program that runs as you can ask Windows to open the secrets as well. An extra secret that is mixed in ("entropy") does not change that, because the program can read where that is kept too; it only stops programs that do not look for PocketPilot. Browsers had the same problem: Chrome now lets a service that runs as SYSTEM check who asks, and stealers have found ways around that too. A separate protection for one program needs things this project does not have: an installer with administrator rights and a certificate for the service, or, for Windows' enclaves, Windows 11 24H2 and a signing account.

What does help, from the strongest:

1. **Do not keep the key on the disk.** In the Claude panel, under the box for the key and under Settings, switch off **Remember the key on this computer**. The Claude key and the GitHub token then stay in memory until you close the program, and the file of the keys has none of them (it only says that they are not remembered, and what was in it is removed when you switch it off). You enter the key again at each start. There is nothing left on the disk for another program to open; a program that reads the memory of PocketPilot while it runs is another matter, which only point 3 helps against. Removing the file's content is not a secure wipe of the disk: a key that was saved before can still be in free space until it is overwritten, so make a new key in the Claude Console if the old one mattered.
2. **Lock the keys with a passphrase** that Windows does not know. A program that runs as you then also needs the passphrase. Not in the program yet either; the second choice after not keeping the key.
3. **Run PocketPilot under its own Windows account.** Other programs of your main account cannot open what that account sealed. This works today and costs nothing.
4. **Make a stolen key cheap.** Give the Claude key its own workspace and a spend limit in the Claude Console, make the GitHub token a fine-grained one for this repository only, with an expiry date, and remember that a Discord webhook can be deleted in the channel settings.

## What Claude may do

- Commits only go to branches named `claude/...`, never with force, never to `pocketpilot`; only text files, never `.github/`, `3rdParty` or `Packages/`; at most 40 files; nothing without your press on **Commit**. Claude cannot merge and cannot make the pull request. This is checked in the code, not only told to Claude.
- In Claude Plays, HOME and saving are off unless you allow them, and the session ends at the number of actions and the cost that you set.
- Read what Claude changes before you commit it: a change to a build file runs when the pull request is built.

## The build

- The build job can only read the repository. The job that publishes can write, runs only for a build that passed, and checks the zip against the SHA-256 that the build made of it before it goes on the release.
- What comes from outside is pinned: the actions by their commit, the Packages repository by its commit, esptool by its version and the SHA-256 of its Windows zip. The firmware that goes in the zip has a list of hashes (`Firmware/SHA256SUMS.txt`), and the program does not flash a file that is not the one that was hashed.
- To move one of them on, change it in `.github/workflows/pocketpilot-windows.yml` on purpose.

## Checking a download

The notes of each release have the SHA-256 of the zip. In PowerShell:

    Get-FileHash .\PocketPilot-Windows.zip -Algorithm SHA256

It has to be the same. The program is not signed with a certificate, so Windows may warn when you start it.

## Known limits

- Not signed, so Windows SmartScreen may warn.
- Claude Plays sends pictures of the game screen to Anthropic. Do not point it at a screen with something private on it.
- The API of Claude is billed by Anthropic on its own: the cost limit of Claude Plays goes by the prices that this build knows.
- The code of Pokémon Automation that this is based on has its own history; its dependencies (Qt, OpenCV, ONNX Runtime, Tesseract) are the versions in the repository and in the workflow.

## Reporting

Open an issue at https://github.com/MajorLOL/Arduino-Source/issues, or write to the owner of the repository. Do not put a key or a token in it.
