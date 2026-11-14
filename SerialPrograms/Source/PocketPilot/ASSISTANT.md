# Claude in PocketPilot Studio

The button "Claude" in the top bar opens a panel at the right of the main window: a conversation with Claude, the AI model of Anthropic. It answers questions about the programs, and it builds or fixes a program in the source of this repository. Its changes never go to the branch the builds are made from: they go to a branch of their own, which you turn into a pull request.

**Status: experimental.** The connection follows Anthropic's and GitHub's documentation as of November 2026. Every build tests it against made-up answers (see "How it is tested" below), but the build machine has no key, so it has not run against the real Claude API or made a real commit until someone with a key tries it. If something does not work, the panel shows what the API answered: that text is what to report.

## What you need

| | |
|---|---|
| **A Claude API key** | From Anthropic's console: the panel links to [how to get one](https://platform.claude.com/docs/en/get-api-key). The API is billed by Anthropic on its own, per token; a subscription to the Claude apps is a different thing and does not pay for it. |
| **A GitHub token** (only for changes) | Reading the source works without. To commit a change and to read why a build failed, Claude needs a fine-grained personal access token for this one repository with the permissions **Contents: Read and write** and **Actions: Read-only**. Make one at [github.com/settings/personal-access-tokens/new](https://github.com/settings/personal-access-tokens/new): "Only select repositories", this repository, those two permissions, nothing else. |

Enter both in the panel yourself: the key on its first page ("Connect Claude"), the token under "Settings". "Test the key" asks the API which models the key can use, and costs nothing.

## Where the key and the token are kept

In `UserSettings/PocketPilot-Assistant-Keys.json`, encrypted with your Windows account (the Windows data protection API): another account or another computer cannot read them. They are never written to the settings file or the log, and Claude itself never gets to see them. While the program runs they are in its memory.

Copying `UserSettings` to the folder of a newer build keeps them. Copied to another computer or account, they cannot be read: the panel then asks for them again.

"Forget the key" under "Settings" removes the key from the file.

## Asking about a program

Claude does not get the whole program with every question. It looks up what it needs with tools, and the panel shows each lookup as a line:

| Tool | What it reads | Network |
|---|---|---|
| `list_programs` | The games and their programs, with what each does. | none |
| `get_open_program` | The program that is open and its options as they are set. Options that look private (a token, a webhook address) are left out. | none |
| `read_log` | The last lines of the program's log: 80, or as many as Claude asks for up to 400, or only the lines with a word. | none |
| `find_files`, `read_file`, `outline_file`, `search_text` | The source, as it is on GitHub: at most 400 lines of a file at a time, the outline of a file, or the lines that match in a folder. | GitHub |
| `branch_status`, `branches` | A `claude/...` branch: how it differs from the base branch, its pull request, and its last build with the compiler's errors. | GitHub |

With every message goes one line that says which program is open and whether it runs. Nothing else is sent by itself, and nothing at all before there is a key.

## Having a program built or fixed

1. Say what to build or fix. Claude reads the code around it and makes the change with `edit_file` or `write_file`. At this point the change exists only in the conversation: nothing is written anywhere.
2. Claude asks to commit: a card "Commit to claude/..." with **Commit**, **View changes** and **Not now**. "View changes" shows every line that goes out and comes in.
3. **Commit** puts the change on the branch `claude/<topic>` of the repository, made from the newest base branch (`pocketpilot`). The card that follows has **Open pull request**: it opens GitHub's page for a new pull request in your browser. Check that it says `base: pocketpilot` of this repository, and create it there.
4. The pull request is built and tested like every build (about 7 minutes with a warm cache, longer for a change in a file that many others include). Ask Claude whether it passed: it reads the result and, if the build failed, the compiler's errors, and fixes them with a further commit on the same branch.
5. Merge the pull request on GitHub. The push to `pocketpilot` makes and publishes the next build.

What the program holds to, whatever Claude asks for:

- Commits go only to branches named `claude/...`, never to `pocketpilot` or any other branch, and never with force. Claude cannot merge and cannot create the pull request.
- Nothing is committed without your click on **Commit**.
- It does not change the workflows (`.github/`), third-party code, the resources (`Packages/`), pictures and other files that are not text, or the generated data of the FRLG AutoStory.
- A file that changed on GitHub after Claude read it is not overwritten: Claude is told to read it again.
- If the base branch has moved on, it is merged into the `claude/...` branch first; if that conflicts, nothing is committed and Claude says so.
- At most 40 files in a commit, and a new source file has to be in `SerialPrograms/cmake/SourceFiles.cmake`.

Claude cannot compile or run anything, on your computer or elsewhere: the pull request's build is the first time a change meets the compiler. It can be wrong. Read the changes before you commit them, and treat a new program as untested until it has run on a console.

## What it costs

Claude's use is counted in tokens (about four characters each) and billed by Anthropic. Under the box you write in, the panel shows what the conversation used so far and what that costs at the public prices this build knows (November 2026, per million tokens of input / output):

| Model | Price | For |
|---|---|---|
| Claude Sonnet 5.5 (default) | $2 / $10 | Questions and most changes. |
| Claude Opus 5.5 | $4 / $20 | A hard bug or a larger design. |
| Claude Haiku 5.5 | $0.10 / $0.50 | Quick questions. |
| Claude Fable 5.2 | $8 / $40 | The most capable, at the highest price. |

The amount is an estimate: Anthropic's invoice is what counts, and prices change. What keeps it low:

- The lookups above happen on your computer and through GitHub; only what Claude asked for is sent on.
- The instructions and the earlier part of a conversation are read from Anthropic's cache at a fraction of the price, as long as the conversation goes on within a few minutes.
- A long conversation is sent again with every message. **New chat** starts a small one: use it for a new subject.
- "Ask before a conversation costs more than" (under "Settings", $1 unless you change it) stops and asks before going on.
- "Effort" (under "Settings") sets how much the model thinks and looks up before it answers.

Set a spending limit in Anthropic's console as well: that one holds whatever this program does.

## What is sent where

- **To Anthropic:** what you write, the line about the open program, and what the tools returned (lines of the log, the open program's options, parts of the source). The log can have the name of your Windows account in a path. How Anthropic handles what the API receives is in its own terms.
- **To GitHub:** the requests to read the repository and, with your token, the commits. A commit is public, like the repository.
- **Nowhere else.** Nothing of a conversation goes to Pokemon Automation or to anyone else, and this fork's builds send no error reports.

## How it is tested

`SerialPrograms --ui-screenshots <folder>`, which every build runs on the build machine, also

- holds a conversation with a made-up Claude and a made-up GitHub behind the same connection the real ones use (a question that reads the log and the source, then a change that ends in a commit), and checks that the change is on a branch `claude/...` and the base branch is as it was;
- checks the parts that need no connection: reading the API's stream (cut into pieces, with thinking blocks and errors), the prices, the line ends and byte order mark of changed files, the errors of a build's log, and that the file with the keys is encrypted and comes back after a new start;
- checks that the build can make secure connections at all.

It uses no key and sends nothing. The pictures `night-fire-claude-*.png` of a release show the panel as that build draws it.

## For developers

The code is in [Assistant](Assistant):

| File | What it is |
|---|---|
| `Assistant_Http` | A request and its answer, behind an interface that the tests replace. |
| `Assistant_Claude` | The Messages API: the request, the stream of the answer, retries, prices. |
| `Assistant_Tools`, `Assistant_Agent` | A tool, and the loop of a conversation: Claude asks for tools, they run, their results go back. |
| `Assistant_GitHub`, `Assistant_Workspace` | The repository: reading, the changes of a conversation, the commit to a `claude/...` branch, the state of a branch and its build. |
| `Assistant_ChatTools` | The tools of the chat. A new tool is one more entry here. |
| `Assistant_Service` | What ties them together, and Claude's instructions. |
| `Assistant_Settings` | The file with the keys and the settings of the panel. |
| `Assistant_Panel` | The panel. |
| `Assistant_SelfTest` | The made-up Claude and GitHub, and the checks of every build. |

Everything runs in the thread of the interface, on Qt's event loop: a request never blocks it. The instructions and the list of tools stay the same for a whole conversation and its earlier messages are never changed, which keeps Anthropic's cache valid and is what the API asks for when the model thinks between tool calls.
