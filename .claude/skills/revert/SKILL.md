---
name: revert
description: Reverts ALL git changes in the working directory: staged changes, unstaged modifications, and new untracked files. Use when user asks to "revert all changes", "undo all changes", "discard all changes", "reset all git changes", or "clean working directory". Do NOT use for reverting a specific file or a specific commit — those need targeted git commands.
disable-model-invocation: true
---

Revert all git changes in the working directory. This is destructive and cannot be undone.

Work through these steps in order.

---

## Step 1 — Show What Will Be Lost

Run these in parallel:
- `git status` (never use `-uall`)
- `git diff --stat` to summarize unstaged changes
- `git diff --cached --stat` to summarize staged changes

If the working tree is clean (no changes, no untracked files), report "Nothing to revert — working tree is clean." and **stop**.

---

## Step 2 — Confirm with User

Display a clear warning listing:
- Modified tracked files that will be reset
- Staged changes that will be dropped
- Untracked files/directories that will be permanently deleted

Ask the user to confirm before proceeding. Stop if they decline.

---

## Step 3 — Reset Tracked Changes

Run:
```bash
git reset --hard HEAD
```

This drops all staged and unstaged modifications to tracked files.

---

## Step 4 — Remove Untracked Files

Run:
```bash
git clean -fd
```

This deletes all untracked files and directories. Ignored files (`.gitignore`) are left intact.

---

## Step 5 — Report

Run `git status` and confirm the working tree is clean. Report to the user:
- How many files were reset
- How many untracked files/directories were removed
- Final status: "Working tree is now clean."

---

## Final Step — Record Usage

Run after the skill's primary task completes:

```bash
python3 ${PWD}/.claude/skills/skill-stat/scripts/record-stat.py "revert"
git add .claude/skill-stats.md
git commit -m "record revert skill usage"
```
