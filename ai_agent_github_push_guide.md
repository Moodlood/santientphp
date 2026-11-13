# SantientPHP --- AI Agent GitHub Push & Repository Hygiene Guide

**Scope:** Git initialization, repository hygiene, commit standards, and GitHub push workflow\
**Applies to:** `santientphp` compiler repository and `.sph` projects (e.g., `pulse_core`)\
**Companion Document:** `AGENT_GUIDE.md`

This guide defines the mandatory rules and step-by-step sequence an AI coding agent must follow when preparing and pushing code to GitHub.

------------------------------------------------------------------------

## 1. Non-Negotiable Safety Rules

Before executing any `git` command, the agent must obey these guardrails:

1. **Never commit build artifacts or binaries:**
   - Do **not** push `build/`, `cmake-build-*/`, `out/`, `*.exe`, `*.o`, `*.obj`, `*.a`, `*.lib`, `*.dll`, `*.so`, `*.dylib`, `*.pdb`, `*.vsix`, `node_modules/`, or temporary `.s` / `.ll` codegen outputs.
2. **Never commit secrets or local machine paths:**
   - Do **not** commit `.env` files, API tokens, SSH keys, credential files, or user-specific IDE configs containing hardcoded `C:\Users\...` paths.
3. **Never rewrite remote history without explicit approval:**
   - Do **not** run `git push --force`, `git push -f`, `git reset --hard`, or `git clean -fdx` unless the user explicitly authorizes it.
4. **Never push a broken build:**
   - Verify that the compiler builds and tests pass before staging and committing changes.
5. **Never invent command outputs or authentication status:**
   - If `git push` fails due to missing credentials or network restrictions, stop and report the exact error message to the user.

------------------------------------------------------------------------

## 2. Pre-Flight Repository & `.gitignore` Audit

Before staging files (`git add`), inspect the workspace state:

1. Check existing Git state:
   ```powershell
   git status
   git branch -a
   git remote -v
   ```
2. Verify that a root `.gitignore` file exists and excludes C++, CMake, Ninja, SantientPHP, VS Code, and OS artifacts. If `.gitignore` is missing or incomplete, create or update it with at least:
   ```gitignore
   # Build directories and CMake / Ninja outputs
   build/
   cmake-build-*/
   out/
   CMakeFiles/
   CMakeCache.txt
   cmake_install.cmake
   build.ninja
   .ninja_deps
   .ninja_log

   # Compiled binaries, libraries, and debug symbols
   *.exe
   *.dll
   *.so
   *.dylib
   *.a
   *.lib
   *.o
   *.obj
   *.pdb
   *.ilk
   *.exp

   # Intermediate assembly / LLVM / packaging outputs
   *.s
   *.ll
   *.bc
   *.vsix

   # VS Code extension & Node dependencies
   editors/vscode/node_modules/
   editors/vscode/out/

   # IDE & OS metadata
   .vs/
   .idea/
   .vscode/*
   !.vscode/extensions.json
   !.vscode/tasks.json
   .DS_Store
   Thumbs.db

   # Secrets and local environment overrides
   .env
   *.local
   ```
3. If build artifacts were previously tracked by Git, untrack them without deleting local files:
   ```powershell
   git rm -r --cached build
   ```

------------------------------------------------------------------------

## 3. Step-by-Step GitHub Push Workflow

Follow this exact sequence:

### Step 1 --- Verify Build & Tests

- For the **SantientPHP compiler repository**:
  ```powershell
  ninja -C build
  .\build\bin\sph_tests.exe
  ```
- For a **SantientPHP project (`pulse_core`)**:
  ```powershell
  sphc check src/main.sph
  sphc run src/main.sph
  ```
- Do not proceed if compilation or tests fail unless the user explicitly requested pushing a work-in-progress branch.

### Step 2 --- Initialize Repository & Remote (If Not Already Configured)

- If `.git` does not exist yet:
  ```powershell
  git init -b main
  ```
- Check `git remote -v`. If `origin` is missing, add the user-provided GitHub URL:
  ```powershell
  git remote add origin <GITHUB_REPO_URL>
  ```
- If `origin` already exists and points to a different URL, confirm with the user before running `git remote set-url origin <GITHUB_REPO_URL>`.

### Step 3 --- Stage & Audit Changed Files

1. Stage files carefully:
   ```powershell
   git add .
   ```
2. Inspect the staged file list and size summary before committing:
   ```powershell
   git status
   git diff --cached --stat
   ```
3. Confirm that:
   - Only source code (`.cpp`, `.hpp`, `.c`, `.h`, `.sph`, `.ts`, `.json`), build scripts (`CMakeLists.txt`, `sph.toml`, `install.ps1`, `install.bat`), tests, and documentation (`.md`) are staged.
   - No single staged file exceeds `50 MB`.

### Step 4 --- Create Clean, Descriptive Commits

Use clear Conventional Commit prefixes (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`):

- For an initial repository push:
  ```powershell
  git commit -m "feat: initial SantientPHP 1.0 compiler, runtime, stdlib, and tooling"
  ```
- For incremental updates, group logically related changes into focused commits rather than one vague commit (e.g., `chore: add .gitignore`, `docs: sync README and development plan`, `feat: add PulseCore verification suite`).

### Step 5 --- Push to GitHub

1. Push the active branch and set upstream tracking:
   ```powershell
   git push -u origin main
   ```
2. **Handling non-empty remote repositories:**
   - If GitHub rejects the push with `error: failed to push some refs` because the remote was initialized with a `README.md` or `LICENSE`:
     ```powershell
     git pull --rebase origin main
     ```
     Resolve any conflicts cleanly, verify `git status`, and re-run `git push -u origin main`.
3. **Handling authentication issues:**
   - Because AI agents run in non-interactive shells, if `git push` prompts for a username/password or fails with `Authentication failed` / `Permission denied (publickey)`, do **not** hang or guess credentials. Report the exact error and instruct the user to authenticate via `gh auth login`, Git Credential Manager, or SSH keys.

------------------------------------------------------------------------

## 4. Mandatory Completion Report

After pushing, report the following to the user:

1. **Remote URL & Branch:** Target GitHub repository URL and branch pushed (e.g., `origin/main`).
2. **Commit Hash & Message:** Output of `git log -1 --oneline`.
3. **File Summary:** Total files committed (`git diff --stat` or `git ls-files | wc -l`) and confirmation that `build/` and binary artifacts were excluded.
4. **Verification Status:** Confirmation that build/tests passed prior to commit, or any warnings/errors encountered.

------------------------------------------------------------------------

## 5. Prompt Template to Give Your Coding Agent

Copy and paste this instruction into your coding agent whenever you want it to push your code:

> Read `GITHUB_PUSH_GUIDE.md` and push this codebase to my GitHub repository at `<PASTE_YOUR_GITHUB_REPO_URL_HERE>` on branch `main`.
>
> Follow every rule in `GITHUB_PUSH_GUIDE.md`:
> 1. Verify the build and test suite pass first.
> 2. Ensure `.gitignore` excludes `build/`, `.exe`, `.o`, `.a`, `.vsix`, and temporary artifacts before staging.
> 3. Audit `git status` and `git diff --cached --stat` so zero binaries or secrets are committed.
> 4. Commit with clean Conventional Commit messages, push to `origin main`, and report the final commit hash and pushed file summary.