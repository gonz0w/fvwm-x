# Static website and manual rewrite

Recorded 2026-10-05; refined by the subsequent developer-document cleanup request.
DOCS-01 now prioritizes OS man-page quality and lightweight optional HTML from
one source. Website visual design remains deferred and does not block hardening,
responsiveness, tool ports or other maintenance adoption.
The [project tracker](../PROJECT-TRACKER.md) owns status and scheduling.

## Current direction

Keep the existing user manual sources and adopted generation path. Improve man
coverage, accuracy, examples, cross-references, portable roff output and staged
OS installation. HTML is optional derived static output; a separate website
framework or mandatory HTML installation is unnecessary. See
[DOCUMENTATION.md](../DOCUMENTATION.md) for generation and qualification.
A fresh render/manual rewrite has not been completed by this document migration.

## Earlier website design request (deferred)

- Replace the current website completely with a modern, static HTML5/CSS
  design. Delivered pages use no JavaScript or server-side application.
- Write new, AI-assisted manuals for FVWM-X and include the generated man
  pages with the distribution.
- Write the manuals using ASD-STE100 Simplified Technical English for clarity.
- Keep one reviewed documentation source for website HTML and installed man
  pages, so fixes and command descriptions stay consistent.
- Preserve FVWM configuration syntax, command names, options, defaults and
  supported theme behavior. A documentation rewrite does not change runtime
  contracts.

## Acceptance to establish when work resumes

Use semantic HTML, responsive CSS, readable typography, accessible navigation,
visible keyboard focus and useful cross-links. Pages must work as static files.
Review the visual design with the user before adopting it.

AI drafts must be checked against the implementation and supported behavior.
Retain literal command/configuration examples and verify their meaning. Cover
core commands, modules, installed tools, Linux/BSD setup and troubleshooting;
keep existing manual names and useful links where compatibility requires them.
Generate and check both HTML and system man-page output from the same source.
The existing AsciiDoc/Meson tooling is a starting point; no new runtime library
is required by this brief.

ASD-STE100 includes writing rules and a controlled dictionary, with provision
for subject-specific technical terms. Select and record the applicable standard
issue and FVWM terminology at kickoff. Review language against those rules
before describing the manuals as compliant; AI drafting alone is not a
compliance check. See the [official ASD-STE100 overview](https://www.asd-ste100.org/about_STE.html).

Website output, deployment destinations and publishing ownership belong to
this later design. No hosting or publication is authorized by recording it.

## Existing candidate

The [fresh-output proposal](WEBSITE-OUTPUT-PROPOSAL.md) and its 28 rendered
review pages remain useful verification artifacts. The proposed public-helper
migration is deferred, not approved. Keep the existing public website helper
available while other maintenance work proceeds. Decide replacement/output
behavior as part of DOCS-01 rather than requiring the old website migration
first.
