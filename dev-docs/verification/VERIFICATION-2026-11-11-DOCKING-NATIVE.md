# Docking native build and installed qualification

PR380 merged as `c1f8b956a5195c139fa974ad5b774017d7bd9f8f` with
its reviewed tree. The earlier live docking, theme and BSD policy
evidence remains applicable.

## Linux ARM64 installed docking

The retained build compiled 16 missing install outputs and completed
a private Meson DESTDIR install. The installed core, desktop controller,
Form, desktop worker, panel modules and staged configuration ran all
nine durable docking cases on Xorg dummy. Staged binary and data hashes
are in the receipt. The run reused 45 earlier command passes and kept
the retained built core and controller bytes. Both owned shutdowns
were confirmed. The editor bounds check ran without a duplicate
screenshot.

The installed Xorg dummy result covers the software-generated mixed
topology. Actual dock and connector behavior requires physical hardware.

## FreeBSD ARM64

The retained dependency and build profile compiled the current Meson
install targets, completed a private stage, and ran the installed core
version check. The authenticated default Xorg started at 800×600 and
refused an unauthenticated client. The existing desktop settings fixture
timed out while waiting for wallpaper at y=850. The build and stage pass
stand independently. The guest shut down and its backing chain was
unchanged.

The focused docking fixture then passed all five cases on authenticated
default X at the same resolution. The fresh child reused the exact
passing staged install. The fixture verified profile publication,
read-only preview, stale workspace refusal, preservation of newer
window placement, and panel and client placement. It captured no
screenshots. The continuation shut down cleanly.

## OpenBSD ARM64

A fresh pinned OpenBSD 8.0 guest built the current installable products,
staged them privately, and passed all five installed docking cases on
authenticated Xenocara default X. It refused an unauthenticated X client.
The fixture used the synthetic FVWM-X-Root role, with no RandR output IDs,
and verified the primary fallback in preview and Apply. The guest shut
down cleanly and the fixture captured no screenshots.

The first run completed signed dependency preparation and Meson
configuration, then stopped on a logical versus physical build path.
The next run built and staged the product and started default X. It
stopped at a fixture expectation for an exact connector on the synthetic
root. The final run retained the exact product source and corrected that
expectation. The runner retires the guest login key at shutdown, so the
last two attempts each used a fresh private stage.

This checkpoint covers native product build and install on ARM64 for
Linux, FreeBSD and OpenBSD, plus authenticated default-X docking on both
BSDs. The Linux installed docking scene uses Xorg dummy. Native AMD64
product, install and default-server gates remain in the support matrix.

## Physical docking

The guest fixtures do not exercise a physical dock or the GPU driver's
connector reporting. Physical qualification still needs a real dock with
two outputs at different scales, unplug and replug while a preview is
pending, connector re-identification after restart, and confirmation that
the panel, windows and pointer remain reachable.

[Results and hashes](VERIFICATION-2026-11-11-DOCKING-NATIVE.json)
