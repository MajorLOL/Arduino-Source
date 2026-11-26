# Claude in Claude GG

The button "Claude" along the top of the window opens a panel at the right of it: a conversation with Claude, the AI model of Anthropic. It answers questions about the programs, and it builds or fixes a program in the source of this repository. Its changes never go to the branch the builds are made from: they go to a branch of their own, which you turn into a pull request. In the program "Claude Plays", Claude plays the game itself.

**Status: experimental.** The connection follows Anthropic's and GitHub's documentation as of November 2026. Every build tests it against made-up answers (see "How it is tested" below), but the build machine has no key, so it has not run against the real Claude API or made a real commit until someone with a key tries it. Claude Plays has run neither with a key nor on a console. If something does not work, the panel (or the program's log) shows what the API answered: that text is what to report.

## What you need

| | |
|---|---|
| **A Claude API key** | From Anthropic's console: the panel links to [how to get one](https://platform.claude.com/docs/en/get-api-key). The API is billed by Anthropic on its own, per token; a subscription to the Claude apps is a different thing and does not pay for it. In the console: "Connect to the Claude API", then "Somewhere else" (the choice that makes a key), and in "Create API key" leave **Scope** on a workspace, as it comes ("Default workspace"): such a key works here as it is, and cannot be used to manage the account. |
| **A GitHub token** (only for changes) | Reading the source works without. To commit a change and to read why a build failed, Claude needs a fine-grained personal access token for this one repository with the permissions **Contents: Read and write** and **Actions: Read-only**. Make one at [github.com/settings/personal-access-tokens/new](https://github.com/settings/personal-access-tokens/new): "Only select repositories", this repository, those two permissions, nothing else. |

Enter both in the panel yourself: the key on its first page ("Connect Claude"), the token under "Settings". "Test the key" asks the API which models the key can use, and costs nothing.

**A key that was not made for one workspace** (another choice under Scope) is not enough by itself: the API wants to be told with each request which workspace to use, and answers "This API key is not scoped to a workspace..." otherwise. For such a key, enter the workspace's ID under "Settings" in the panel ("Workspace ID": it starts with `wrkspc_`, and the console's page [Workspaces](https://platform.claude.com/settings/workspaces) shows it). Leave that box empty for a key of one workspace. The ID is not a secret: it is kept with the panel's other settings.

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

## Letting Claude play

The program "Claude Plays" lets Claude play toward a goal you write down, in any language: something no program exists for. It is in the list of every game, in the section "Claude", and the list **Other Games** has it for a game without a list of its own: there Claude finds out from the screen which game it is, or you tell it under "About the game". It uses the key of the panel. Write the goal in the program's options and start it like any other program.

Claude does not watch the video. It gets tools, and the program runs each call on the console. In every game:

| Tool | What the program does |
|---|---|
| `look` | Sends a picture of the screen, or of a part of it (top, bottom, left, right, center, a corner) at twice the detail. |
| `read_text` | Sends the text on the screen, or in a part of it, as the program reads it (the OCR of the other programs). |
| `press` | Presses buttons one after the other, up to 20 in one call. |
| `hold` | Holds buttons and sticks together for a time: walking and the camera in a 3D game, a button held to run. |
| `mash` | Presses one button again and again for some seconds: long texts. |
| `wait` | Waits for a time, until the picture is still (a loading screen, a scene that plays by itself), or until it changes. |
| `save_game` | Saves with the routine of the game's other programs. Only in Sword and Shield, Brilliant Diamond and Shining Pearl, Legends: Arceus, Scarlet and Violet and Legends: Z-A, and only when you allow saving. |
| `note`, `done` | Writes a note down for itself; ends the session. |

Every result ends with what the program sees by itself: how much of the picture changed through the action, and whether it still moves ("Screen: 21% of the picture differs from before the action; still now."). That tells whether a press did anything, for a few tokens. A picture or the text can also be asked for with an action, so that seeing what a press did needs no call of its own.

In **FireRed and LeafGreen** the tools are those of the AutoStory instead, and every result says what its detectors find on the screen: a text box and its text, a YES/NO question, a battle, the start menu.

| Tool | What the program does |
|---|---|
| `look` | Sends a picture of the game. |
| `press` | Presses buttons (A, B, START, SELECT, L, R and the four directions), up to 20 in one call. |
| `walk` | Walks up to 20 steps one way, each step checked on the video. A battle that starts on the way is fought to its end. |
| `battle` | Fights the battle on the screen to its end the way the AutoStory does (the move that does the most damage, new moves learned when they are better), or runs from a wild one. |
| `advance_text` | Gets through text and scenes until the player can move again. |
| `close_menus` | Closes menus and text until the player stands free. |
| `heal_at_nurse` | In a Pokémon Center: up to the nurse, and the party healed. |
| `save_game` | Saves the game. Only there when you allow saving. |
| `wait`, `note`, `done` | Waits; writes a note down for itself; ends the session. |

What each list's Claude Plays tells Claude: the name of its game and its main controls, as far as they are certain (Claude is told to go by the buttons the screen shows). Let's Go, Pikachu! and Let's Go, Eevee! are played with their single right Joy-Con; catching there takes a throwing motion, which the program cannot make. In Other Games, Claude is told that the program does not know the game.

What keeps a session cheap:

- Text instead of pictures: the line about the screen after every action, and `read_text` for dialogs and menus. A picture of the whole screen is 640 x 360 unless you choose another size (about 299 tokens), packed as a JPEG. In FireRed and LeafGreen it is the game's own 240 x 160 pixels doubled (about 216 tokens), and the program's own reading of the screen comes with every result.
- One call for many presses, a held stick, a mashed button, a wait for the end of a loading screen. In FireRed and LeafGreen the AutoStory's routines do a whole battle or scene for one call, without a token in between.
- The smallest model, Claude Haiku 5.5, is the default.
- A conversation that has grown to 40,000 tokens, or to 3 MB of pictures, is started over with Claude's own notes and the last six actions, so that a long session does not send its whole past with every request.

It ends when Claude says it is done, after the number of actions you set (100 unless you change it), at the estimated cost you set ($0.50 unless you change it), or when you stop the program. By the prices below, a hundred actions come to a few cents with the smallest model and to some tens of cents with Sonnet 5.5. That is arithmetic, not a measurement: no session has run yet.

What to expect: it is slow next to a program made for one task, since every decision is a request to Claude, and it can lose its way. From a picture a model does not always tell where things are, and in a 3D game walking somewhere with held sticks takes many small steps and looks; the smallest model is the weakest at that. What the program reads from the screen can be wrong too, which is why Claude can ask for the picture. Text is read as English.

Saving is off unless you allow it: Claude is told not to save and has no tool for it, and in FireRed and LeafGreen the program does not press A while the game asks whether to save. Claude still presses the buttons, so this cannot rule a save out altogether, and **many games save by themselves** (Scarlet and Violet, Legends: Arceus and Tears of the Kingdom among them): what Claude does there can be final. Save the game yourself before you start the program, or back it up, where that matters. HOME, which leaves the game for the console's menu, is not pressed unless you allow that.

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

- **To Anthropic:** what you write, the line about the open program, and what the tools returned (lines of the log, the open program's options, parts of the source). From Claude Plays: the goal and what you wrote about the game, what the program read from the screen, and the pictures of the game Claude asked for. The log can have the name of your Windows account in a path. How Anthropic handles what the API receives is in its own terms.
- **To GitHub:** the requests to read the repository and, with your token, the commits. A commit is public, like the repository.
- **Nowhere else.** Nothing of a conversation goes to Pokemon Automation or to anyone else, and this fork's builds send no error reports.

## How it is tested

`SerialPrograms --ui-screenshots <folder>`, which every build runs on the build machine, also

- holds a conversation with a made-up Claude and a made-up GitHub behind the same connection the real ones use (a question that reads the log and the source, then a change that ends in a commit), and checks that the change is on a branch `claude/...` and the base branch is as it was;
- checks the parts that need no connection: reading the API's stream (cut into pieces, with thinking blocks and errors), the prices, the headers of a request with and without a workspace ID and what the panel says when the API misses one, the line ends and byte order mark of changed files, the errors of a build's log, and that the file with the keys is encrypted and comes back after a new start;
- checks that the build can make secure connections at all;
- plays three turns from a thread of its own, as Claude Plays does (a look with its picture, a press, the end);
- plays a whole session of the Claude Plays of every list on a console that is made up for the test (a look as a JPEG, presses with the text read after them, a stick held, a note, done), and checks what the program says about the screen after each action;
- checks that every tool of Claude Plays has a schema the API takes: those of FireRed and LeafGreen, and those of every other list with its controller, with and without saving and HOME.

It uses no key and sends nothing. The pictures `night-fire-claude-*.png` of a release show the panel and the pages of Claude Plays (of FireRed and LeafGreen, and of Other Games) as that build draws them.

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
| `Assistant_Play` | For a program in which Claude plays: the program waits in its own thread for Claude's next call, runs it, and hands back what came of it. No Qt in its header. |
| `Assistant_SelfTest` | The made-up Claude and GitHub, and the checks of every build. |

Claude Plays is in [ClaudePlays](ClaudePlays), on top of `Assistant_Play`:

| File | What it is |
|---|---|
| `ClaudePlays_Engine` | The session every game shares: the goal and the tools go to Claude, each call is run, Claude's notes and the last actions are kept, the conversation is started over when it has grown, the limits on actions and cost. A game brings its tools (`Game`), a program its log and numbers (`Host`). |
| `ClaudePlays_AnyGame` | The tools for a game without routines of its own (look, read_text, press, hold, mash, wait), on a console behind an interface (`PlayConsole`), so that they run in a test without one. How two pictures are told apart is here too. |
| `ClaudePlays_Games` | What each list's Claude Plays tells Claude about its game. A new game is one more entry. |
| `ClaudePlays_Program` | The program itself, the console with a Pro Controller and with a Joy-Con, the games' own save routines, and the list Other Games. |

`VideoGames/PokemonFRLG/Programs/ClaudePlays` is the Claude Plays of FireRed and LeafGreen: the same session, with the AutoStory's routines as its tools.

Everything runs in the thread of the interface, on Qt's event loop: a request never blocks it. (A program in which Claude plays runs in its own thread, like every program, and reaches the conversation through `Assistant_Play`.) The instructions and the list of tools stay the same for a whole conversation and its earlier messages are never changed, which keeps Anthropic's cache valid and is what the API asks for when the model thinks between tool calls.
