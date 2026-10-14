# Website helper output decision

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: deferred by the user on 2026-10-05. The complete static website and
manual rewrite is now tracked as [DOCS-01](WEBSITE-MANUALS-BRIEF.md). This
proposal is retained as an implemented candidate; it is not an active blocker
or an approved public-helper behavior change.

The existing `meson-scripts/generate-manpages-website.sh SOURCE` requires an
existing `Man/` directory in the caller's working directory and overwrites
its generated pages. It also writes and then deletes intermediate `.ad`
files under `SOURCE/doc`. The public entry point remains unchanged pending
this decision.

The replacement is implemented for explicit review:

```sh
python3 tools/maintenance/docs.py --source /path/to/fvwm3 \
    --output /path/to/fresh/Man --format html --website \
    --website-directory --prompt
```

It prepares all pages in temporary storage, then publishes a flat `Man/`
directory only after successful rendering. It refuses an existing directory
or symlink and leaves the source documentation untouched. It preserves the
legacy page front matter, links, index title/version and Prompt page, and adds
the previously omitted desktop manual. It does not publish a website.

Recommended decision: make the public helper use this fresh-directory
behavior, with `Man/` as its default destination and an optional explicit
output path. Repeat builds use a new directory; callers replace or deploy
completed output through their existing publishing process. No automatic
recursive removal or replacement is proposed.

The alternative is to preserve overwrite behavior for the public helper,
while keeping the fresh-output renderer available explicitly. That requires
an additional replacement path and its failure/symlink checks.

This is a caller-visible behavior change: a workflow that creates `Man/`
before invoking the old helper would need to stop creating it, or select a
new destination. AGENTS.md requires an explicit user decision for a specific
compatibility break. No FVWM theme, configuration, desktop binding or runtime
command is changed by either choice.
