# Optional locking and Autorandr recipe review — 2026-10-06

Status: **source/documentation review complete; runtime acceptance unrun**.
Owner: user-requested configuration contribution; execution order unchanged.

## Scope

- `contrib/locking/`: xlock resources and named manual function; commented
  OpenBSD xidle startup with an initial-login guard.
- `contrib/displays/`: named Autorandr actions and commented initial-login
  detection; real profiles must be captured by the user.
- `contrib/README.md`, the optional-recipe section in `doc/FvwmDesktop.adoc`,
  and the tracker progress entry.

No runtime implementation, default-config change, dependency/installation rule,
VM/build artifact or another thread's source change was made for this work.
No locker, idle observer, resource merge or display-changing command was run.

## Review and local checks

Reviewed FVWM's documented `AddToFunc StartFunction` / `Test (Init)` pattern in
`doc/fvwm3_manpage_source.adoc`, the baseline `SlateTopology` callbacks, and
the current locker/power discovery in `default-config/session.py`. Active recipe
lines only define named functions and their deferred `Exec` bodies; the startup
and binding examples remain comments. Reading either recipe launches nothing.

Six contrib files passed local link-existence, LF/final-newline and trailing
whitespace checks. FVWM recipe bodies were inspected: three active lines for
locking and nine for displays, all function definitions/deferred actions.
`git diff --check` reported no issues on the selected tracked paths; the new
contrib files were checked directly because they are untracked.

Options/activation were reviewed against the
[OpenBSD xlock manual](https://man.openbsd.org/xlock.1),
[OpenBSD xidle manual](https://man.openbsd.org/xidle.1), and
[Autorandr upstream usage](https://github.com/phillipberndt/autorandr).
Linux availability was checked in
[Fedora's package catalogue](https://packages.fedoraproject.org/pkgs/xlockmore/xlockmore/);
FreeBSD availability is listed by
[FreshPorts](https://www.freshports.org/x11/xlockmore/).
Direct FreeBSD source retrieval was blocked by the source site's challenge;
that catalogue is availability evidence, not a build/authentication qualification.
No universal Linux/BSD base-package availability is asserted.

## Remaining acceptance and limits

No FVWM parser run, native OS locker authentication/grab check, physical display
hotplug/resume check or multi-platform runtime gate was performed. Each recipe's
README lists the required local checks. The local shell has no `asciidoctor` on
PATH; the new AsciiDoc paragraphs were reviewed as source, not rendered into
a man page in this checkpoint. Existing manuals/build outputs were not replaced.

These recipes do not implement lifecycle supervision, an elogind backend,
sleep inhibitors, lock-ready acknowledgement or lock-before-suspend ordering.
Those remain session-design discussion. Native notification/tray roadmap work
and the deferred native locker retain their existing disposition.
