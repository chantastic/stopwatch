# Figma page audit — September 17, 2026

This follows the initial [design verification](design-verification-2026-09-17.md)
and the subsequent [source typography inspection](../firmware/factory_badge/main/ui/fonts/README.md).
The reference is the user's [init() Figma file](https://www.figma.com/design/H0uyTnXh5IkKwiZRTL7QRo/init---conf-2026?node-id=7307-612)
and its eight supplied screen exports. Source frames are 466×466; they are centered
with a one-pixel horizontal offset in the badge's 468×466 framebuffer.

## Page comparisons

| Source frame | Verified or corrected |
| --- | --- |
| Intro Screen looping | Exact large mark and arrows; corrected shared pagination spacing and vertical position. Retains the separately supplied moving background and requested pure black. |
| Schedule highlighted | 273px cards, 8px square cutouts and 2px selected outline; corrected content padding and vertical rhythm. Exact source bookmark outline replaces the earlier approximation. Moving its tap target onto the card fixes clipping below the time header. |
| Developers After Dark | Corrected cream heading and text baselines; checked reserved QR box and footer positions. The unscannable invitation placeholder remains until the URL is supplied. |
| Profile | Verified 160px portrait, configure-button bounds and supplied placeholder. Corrected name/company baselines and button color. |
| Profile - Filled | Verified portrait and text geometry with synthetic profile data. Retains the later requested uncluttered configured face, dynamic photo, and profile QR interaction. |
| Make it yours | Retains the requested “Hack this device.” / “Learn how” copy. Uses the exact supplied 222px QR artwork through a native LVGL image; verified destination and compile-time URL guard. |
| Settings | Corrected control text sizes/baselines, geometric plus/minus marks, divider color/dash cadence, and hint placement. Retains 60% default, Default orientation, 12-hour clock and reset/touch controls. |
| Connect phone | Verified 162px QR and Go back bounds. Corrected caption baselines and white credential values beside gray prefixes using native flex labels. QR contents remain generated from actual setup credentials. |

Shared source colors are `#F5F2E8` for cream text, `#808080` for muted white and
`#161616` for panels. The small supplied logo already contains 50% alpha; its
coverage is preserved without applying another opacity reduction. Pagination
uses 8px gaps between 8px inactive and 12px active squares, at Y=431 and Y=429.
The supplied left arrow is shifted one pixel to account for source centering.

The renderer remains LVGL 9.5.0 with Smooth and Mooncake. Native labels, buttons,
flex layout, scrolling, image widgets, dynamic QR and GIF playback own these
views. No alternate rendering or layout framework was introduced.

## Host verification

The production native UI suite passed with address/undefined-behavior sanitizers.
It covers all pages and modals, tap/drag cancellation, scrolling, bookmarks,
profile/QR lifecycle, reset gates and idle rendering. New pixel assertions cover
all four selected-card cutouts and the bookmark's formerly clipped bottom notch.
All four 10×10 selected-corner regions match the source silhouette with zero
mismatches.

Fresh renders of all source pages, alternate schedule states, configured profiles,
expanded QR, setup, touch and reset states were reviewed. Synthetic Wi-Fi,
profile and Hack QR images decode. The fixed Hack QR's opaque black/white pixels
match the source exactly; its antialiased edges differ by at most 11/255 per
channel after LVGL alpha blending and RGB565 conversion in the host renderer.
Asset regeneration preserves the supplied alpha masks without resampling; the
outline bookmark reconstructs its source composite without pixel error.

The ESP32-S3 build passed. Application size is 2,423,168 bytes with 722,560 bytes
remaining in the existing application slot. Application SHA-256:
`dffe7a016670a6bae91efcabfb6f2f614a5bb4a13b826c887cf28237a3f4fb94`.

## Connected StopWatch verification

The guarded upload passed partition preflight, write-hash verification and the
clock/storage readiness gate without initializing storage. Public framebuffer
captures verified Schedule, Hack and Settings. The Hack QR decoded and its
opaque module pixels matched the source exactly; antialiased pixels differed
by at most 11/255 per channel, as in the host renderer. Intro captures advanced
and retained pure-black background pixels.

Saved profile-presence indicators, account mask, photo presence, bookmarks,
After Dark unlock, brightness, orientation and network preference were unchanged.
The badge remained at 60% brightness, Default rotation 0, with valid clock and
radios off. Its original page (intro) was restored. No personal screen was
captured and no reset was invoked. This verifies native framebuffer output;
it does not measure physical panel color calibration or fingertip alignment.

## Fidelity boundaries

This is not a claim that every live-text pixel equals Figma. Inter Medium remains
the user-approved substitute for Suisse Intl Medium. Native LVGL glyph
rasterization, whole-pixel tracking/line boxes and RGB565 color quantization
also differ from Figma, including for IBM Plex Mono. Caption/name baselines were
adjusted against visible source pixels while retaining native text rendering.
Source fractional tracking rounds to zero and 20.8px line boxes round to 21px.

Real agenda contents, actual time/battery/settings, profile data and generated
Wi-Fi/social QR patterns deliberately vary from the sample. The invitation URL
is still pending. The intro uses the supplied video rather than the still frame's
particular phase. Reset and touch modals have no corresponding supplied frame;
they were checked for consistent styling and complete content.

Screenshots use synthetic profiles or public device pages. Private renders,
source comparisons, status and logs remain under `.build/pixel-audit/`; generated
firmware and live diagnostics are not committed.
