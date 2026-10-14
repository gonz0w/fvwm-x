---
phase: 04
slug: wm-owned-ui-metrics-and-resources
status: verified
threats_open: 0
asvs_level: 1
created: 2026-08-31
---

# Phase 04 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Style/config source integers → pixel conversion | Untrusted TitleStyle, BorderWidth, EdgeThickness, and image dimensions enter checked scale arithmetic. | Config integers, scaled X11 pixels |
| Cache key identity → resource reuse | A colliding key would serve the wrong scale's font or pixmap. | Font names, picture paths, scale extras |
| Font name string → Xft/XLFD open | Config-supplied names select faces and log text. | Font name bytes |
| Window slot → process-global DefaultFont | Unloading the shared 1x face while other slots hold it is use-after-free. | FlocaleFont pointers |
| Image/cursor path → decoded raster + hot spot | User paths supply dimensions and xhot/yhot. | File bytes, ARGB, hotspot |
| AddWindow → X server | Extra AtScale bind runs on the single X thread during map. | Font load on the WM event thread |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-04-01 | Tampering | Picture/Flocale/icon/cursor cache | high | mitigate | Lookup requires scale_quarters, target size, and generation (`Picture.c`, `Flocale.c`) | closed |
| T-04-02 | Denial of Service | wm_ui_scale_px overflow / wrap | high | mitigate | Outputs init to 0; OVERFLOW/INVALID_ARGUMENT; last-good kept (`wm_ui_metrics.c`) | closed |
| T-04-03 | Tampering | wm_ui_scale_hotspot | high | mitigate | Convert then clamp to [0, extent-1] (`wm_ui_metrics.c`, `PictureImageLoader.c`) | closed |
| T-04-04 | Tampering | protocol units | medium | mitigate | Converter returns actual pixels only; `vpacket.h` / status JSON unscaled | closed |
| T-04-05 | Denial of Service | decoded image dimensions | high | mitigate | `FImageSizeIsSafe` retained on PNG/SVG/XPM loaders | closed |
| T-04-06 | Tampering | SVG decoder | medium | mitigate | Keep librsvg; no second parser; size checks remain | closed |
| T-04-07 | Repudiation | font name in logs | low | accept | Existing Flocale diagnostics; no new unescaped path | closed |
| T-04-10-01 | Denial of Service | bind_default_font_at_scale | high | mitigate | NULL AtScale keeps `Scr.DefaultFont`; do not abort the window | closed |
| T-04-10-02 | Tampering | destroy_window_font / destroy_icon_font | high | mitigate | Unload only when slot pointer is not `Scr.DefaultFont` | closed |
| T-04-10-03 | Tampering | CMD_DefaultFont | medium | mitigate | Rebind USING_DEFAULT_* windows before unloading previous face | closed |
| T-04-10-04 | Information Disclosure | fallback->name in Flocale | low | accept | Font names already cross `fvwm_debug` | closed |
| T-04-11-01 | Denial of Service | mapped_edge_thickness / HandlePaging | high | mitigate | Conversion failure keeps source; source 0 stays disabled; EdgeThickness 0-2 | closed |
| T-04-11-02 | Tampering | last_edge_thickness | medium | mitigate | Store `source_thickness` only so mapped pixels are never scaled twice | closed |
| T-04-11-03 | Elevation of Privilege | PanFrame InputOnly windows | low | accept | Existing InputOnly pan frames; this phase only sizes and hit-tests them | closed |
| T-04-12-01 | Tampering | DefaultFont cache-hit UnloadFont | high | mitigate | Extra count dropped; helper still returns fallback; destroy skips `Scr.DefaultFont` | closed |
| T-04-12-02 | Denial of Service | setup_*_font | high | mitigate | NULL AtScale keeps fallback; do not abort AddWindow | closed |
| T-04-12-03 | Denial of Service | AddWindow post-ownership AtScale | medium | mitigate | Reuse `FlocaleLoadFontAtScale` only; no new filesystem/subprocess work | closed |
| T-04-12-04 | Information Disclosure | fallback->name in Flocale | low | accept | Font names already cross `fvwm_debug` | closed |
| T-04-SC | Tampering | package installs | high | accept | No npm/pip/cargo installs in this phase | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| R-04-SC | T-04-SC | Phase adds no npm/pip/cargo installs; Meson flags enable already-detected system libraries. | plan-time accept | 2026-08-31 |
| R-04-07 | T-04-07 | Existing Flocale diagnostics already include module and name; no new unescaped path added. | plan-time accept | 2026-08-31 |
| R-04-10-04 | T-04-10-04 | Font names already cross fvwm_debug through existing Flocale load paths. | plan-time accept | 2026-08-31 |
| R-04-11-03 | T-04-11-03 | Existing InputOnly pan frames; this phase only sizes and hit-tests them. | plan-time accept | 2026-08-31 |
| R-04-12-04 | T-04-12-04 | Font names already cross fvwm_debug through existing Flocale load paths. | plan-time accept | 2026-08-31 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-08-31 | 19 | 19 | 0 | gsd-secure-phase (L1, register authored at plan time) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-08-31
