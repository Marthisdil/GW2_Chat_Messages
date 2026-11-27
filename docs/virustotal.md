# VirusTotal scan of every release

Every release's DLL is uploaded to [VirusTotal](https://www.virustotal.com), which checks it
with about 70 antivirus engines. The GitHub release's text gets the DLL's SHA-256 and a link to
the report, so anyone can check the file they download. It runs by itself in the release
workflow; this page says how to set it up (once), how it decides, and how to reuse it in
another project.

## The pieces

| File | What it does |
|---|---|
| `tools/virustotal_scan.ps1` | Uploads files, waits for the scan, writes a Markdown table (for release notes) and a JSON summary. Self-contained: copy it into any project as it is. Windows PowerShell 5.1 or PowerShell 7 (Windows, Linux, macOS). |
| `.github/workflows/release.yml` | On a version tag: builds, runs the tests, scans, and publishes the GitHub release with the changelog section, the scan table and the build stamp. |
| `tools/release_notes.ps1` | Takes that version's section out of `CHANGELOG.md` for the release text. |

## Setting it up (once per GitHub account and repository)

### 1. A VirusTotal account and API key

1. Go to https://www.virustotal.com and click **Sign up** (top right). A free **Community**
   account is all this needs. Use any email you like; confirm it from the email they send.
2. Sign in, click your name or avatar (top right) and choose **API key**. The page shows your
   personal key (a 64-character hex string) with a copy button.
3. Keep the key private: anyone who has it can use your account's quota. Don't paste it into
   chat, an issue, a commit or a file in the repository. If it ever leaks, the same page lets you
   reset it (and then update the GitHub secret below).

One key is enough for all your projects: the limits below are per account, not per project.

### 2. The key as a GitHub Actions secret

In the repository on GitHub: **Settings > Secrets and variables > Actions > Secrets > New
repository secret**:

- **Name:** `VT_API_KEY`
- **Secret:** the key from step 1.

Or from a terminal in the repository (it asks for the value, so it never lands in your shell
history):

```powershell
gh secret set VT_API_KEY
```

GitHub never shows a secret again after it's saved, and hides it in the workflow logs. The
release workflow stops at its first step, before building, if the secret is missing.

To use the same key in several repositories without adding it to each, save it as an
**organization** secret instead (only for repositories under an organization), or simply repeat
this step per repository.

### 3. Optional: how many engines may flag it

**Settings > Secrets and variables > Actions > Variables > New repository variable**:
`VT_MAX_FLAGS`, a number (default **3**). If more engines than that call the DLL malicious or
suspicious, the release is saved as a **draft** for you to look at (below).

### 4. Try the key locally (optional)

```powershell
$env:VT_API_KEY = Read-Host "VirusTotal API key"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\virustotal_scan.ps1 -Path build\Release\GW2_Chat_Messages.dll
```

This uploads the file, so the file becomes visible on VirusTotal (see "What gets shared"): use
a build you're happy to share. `Remove-Item Env:VT_API_KEY` afterwards.

## How a release runs

1. Prepare the release as before: `tools\set_version.ps1 -Channel Dev`, commit `src/Version.h`
   and `CHANGELOG.md` in a PR, merge it.
2. Tag the merged commit with the version's numbers and push the tag:

   ```powershell
   git tag 2026.11.21.1412 <sha on main>
   git push origin 2026.11.21.1412
   ```

3. The **Release** workflow (Actions tab) then:
   - checks the tag is a release tag, matches `CHATMSG_VER_TAG` in `src/Version.h`, and is on
     `main`, and that the `VT_API_KEY` secret is set;
   - builds Release, checks the `GetAddonDef` export and runs the unit tests;
   - takes the version's section from `CHANGELOG.md` (it stops if there's none);
   - uploads the DLL to VirusTotal and waits for the result (usually 1 to 5 minutes; at most 20);
   - publishes the GitHub release: title = the version text (`2026.11.21.1412.Dev`), the DLL
     attached, a pre-release for Dev and Beta, a normal release for Rel. Its text is the
     changelog section, then the scan table (file, SHA-256, "0 of 70 engines flagged it" linked
     to the report, and any engine that flagged it), then the build stamp.
4. Nexus' updater sees the new release.

A **published** release is never changed: players already have its DLL, and a rebuild would
be a different file (the build stamp's time changes its SHA-256). A fix needs a new version.
Only a **draft** (one held for review, below) can be re-run: its DLL and text are replaced, and
it's published if the new scan is clean. To re-run for a tag that's already pushed: Actions >
**Release** > **Run workflow** > enter the tag.

## A test release

To check the scan and the release text without releasing a version: Actions > **Release** >
**Run workflow**, **Use workflow from: main**, leave the tag empty, tick **test**, **Run
workflow**. It builds `main` as it is, tags it `test-<short sha>`, takes the notes from
`[Unreleased]`, and publishes a pre-release titled "TEST ..., to be deleted" with a warning at
the top. `CHANGELOG.md` and the version aren't touched, and Nexus ignores the tag (it isn't
numbers only). Delete it afterwards, tag included:

```powershell
gh release delete test-<short sha> --cleanup-tag --yes
```

The DLL stays on VirusTotal (uploads can't be taken back), which is fine for a build of public
code.

## When it's held as a draft

The release is saved as a **draft** (not visible to players, not offered by Nexus) and the
workflow run fails with the reason when:

- more engines than `VT_MAX_FLAGS` flagged the DLL, or
- the scan didn't finish in 20 minutes.

Then:

1. Open the draft (Releases page, or the link in the run) and follow its VirusTotal link.
2. Look at which engines flagged it and what they call it. A few generic or "heuristic"
   detections (names like `Generic`, `ML`, `Heur`, `Suspicious`, `Malicious (score: ...)`) are
   common false positives for unsigned DLLs that hook a game's window or send keys; a detection
   from many well-known engines, or a named malware family, isn't. When in doubt, don't publish:
   build again from a clean checkout and compare.
3. If it's a false positive: **Edit** the draft and click **Publish release**. You can also
   report the false positive to that engine's vendor (each has its own form or address;
   VirusTotal's documentation keeps a list of vendors' false-positive contacts), then click
   **Reanalyze** on the report a few days later.
4. If the scan only timed out: open the link later; once the report is there, publish the draft
   (or re-run the workflow for the tag, which rebuilds and scans again).

## What gets shared

- The DLL is uploaded to VirusTotal: their community (and paying customers, who can download
  files) can see it. That's fine for a file you're publishing on GitHub anyway; never scan
  private files this way.
- The free (public) API allows **4 requests a minute** and **500 a day**. One release uses about
  5 to 20 requests (a lookup, the upload, a check every 20 seconds). The script waits between
  calls and backs off for a minute if VirusTotal says it's too busy.
- VirusTotal's terms: the public API must not be used in commercial products or services. A
  free, open-source addon is fine; a paid product needs their premium API.

## Reusing it in another project

1. Copy `tools/virustotal_scan.ps1` into the project's `tools/` (no changes needed).
2. Add the `VT_API_KEY` secret to that repository (step 2 above; the same key works).
3. In that project's release workflow, after the build:

   ```yaml
   - name: VirusTotal scan
     shell: pwsh
     env:
       VT_API_KEY: ${{ secrets.VT_API_KEY }}
     run: |
       ./tools/virustotal_scan.ps1 -Path path/to/file.dll, path/to/other.exe -MarkdownOut vt.md -JsonOut vt.json
       if ($LASTEXITCODE -eq 1) { throw "VirusTotal scan failed" }
       exit 0
   ```

   then add `vt.md` to the release text (`gh release create ... --notes-file`). `vt.json` has
   per file: `Name`, `Sha256`, `Link`, `Status` (`completed` or `timeout`), `Malicious`,
   `Suspicious`, `Undetected`, `Harmless`, `Engines`, and `Flagged` (the engines that flagged
   it), for the workflow to decide whether to hold the release. This repository's
   `.github/workflows/release.yml` is a full example (the hold as a draft, pre-releases, re-runs).
4. Exit codes: `0` all scans finished, `2` a scan didn't finish in time, `1` an error (no key,
   a missing file, the API refused). A file over 32 MB needs VirusTotal's large-file upload,
   which the script doesn't do.
