"""PreToolUse hook: never stage or commit the owner's private test notes.

`docs/testing results.txt` stays untracked on purpose. This blocks (exit 2) a Bash or PowerShell
command that would add it to git: one that names it in a git add/commit/stage command, a
`git add` of everything (-A, --all, ".", "docs", "*", ":/"), or a `git commit` while it's
already staged. Anything else passes (exit 0). A hook error also passes, so a broken hook never
blocks normal work.
"""
import json
import os
import re
import subprocess
import sys

PRIVATE = 'docs/testing results.txt'

# A `git ... add` and whatever follows it up to the next ; & | or newline.
ADD_RE = re.compile(r'\bgit(?:\s+-[cC]\s+\S+|\s+--?\S+)*\s+add\b([^;&|\n]*)')
BROAD = re.compile(r'(?:^|\s)(?:-A|--all|-[a-zA-Z]*A[a-zA-Z]*|\.|\./|docs|docs/|\./docs/?|\*|:/|:)(?=\s|$)')
COMMIT_RE = re.compile(r'\bgit(?:\s+-[cC]\s+\S+|\s+--?\S+)*\s+commit\b')


def block(reason):
    sys.stderr.write('Blocked: ' + reason + ' ("' + PRIVATE + '" must stay untracked: '
                     'stage files by name instead.)\n')
    sys.exit(2)


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        return 0
    cmd = (data.get('tool_input') or {}).get('command') or ''
    if 'git' not in cmd:
        return 0
    norm = cmd.replace('\\', '/')

    if 'testing results' in norm and re.search(r'\bgit\b[^;&|\n]*\b(add|commit|stage|mv)\b', norm):
        block('this git command names the private notes file.')

    for m in ADD_RE.finditer(norm):
        if BROAD.search(m.group(1)):
            block('"git add' + m.group(1).rstrip() + '" would stage everything, the private notes too.')

    if COMMIT_RE.search(norm):
        root = os.environ.get('CLAUDE_PROJECT_DIR') or os.getcwd()
        try:
            staged = subprocess.run(['git', '-C', root, 'diff', '--cached', '--name-only'],
                                    capture_output=True, text=True, timeout=10).stdout
        except Exception:
            staged = ''
        if PRIVATE in staged.splitlines():
            block('the private notes are staged; run git restore --staged on them first.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
