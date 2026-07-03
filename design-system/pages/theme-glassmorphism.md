# Theme Page Spec - Glassmorphism OBS/Aitum

**Parent design system**: `design-system/MASTER.md`
**Scope**: Complete implementation guide for the dark glassmorphism OBS/Aitum theme
**Implemented theme**: `theme/Aitum_Glass.obt`, `theme/Aitum_Glass_Default.ovt`, and `theme/Aitum_Glass/`

---

## Theme Goal

Create a polished dark glassformismo theme for OBS/Aitum that feels like a professional live-production control surface: calm, sharp, readable, and premium. The theme should support long sessions, fast source scanning, visible output state, and keyboard-heavy operation.

Glassmorphism is expressed through:

- layered dark surfaces,
- high-opacity glass fills,
- crisp translucent-looking edges,
- subtle highlight sheens,
- compact status lights,
- AA-readable text,
- explicit focus rings.

The theme must remain usable when the rendering stack only supports ordinary Qt stylesheet colors.

---

## Theme Metadata

```css
@OBSThemeMeta {
    name: 'Aitum Glass';
    id: 'com.obsproject.Aitum.Glass';
    author: 'Aitum + Codex';
    dark: 'true';
}
```

The variant metadata extends the same id through `theme/Aitum_Glass_Default.ovt`.

---

## Material Model

| Layer | Token | Purpose |
|-------|-------|---------|
| `canvas` | `--bg_window` | Main application background |
| `base` | `--bg_base` | Generic content backing |
| `preview` | `--bg_preview` | OBS preview/canvas well |
| `glass-1` | `--glass_fill_subtle` | Dock bodies, settings pages |
| `glass-2` | `--glass_fill` | Toolbars, title strips, inactive controls |
| `glass-3` | `--glass_fill_hover` | Hovered controls and raised panels |
| `glass-4` | `--glass_fill_pressed` | Pressed controls, active sheets |
| `edge` | `--glass_edge_soft` | Default component borders |
| `edge-strong` | `--glass_edge_strong` | Hover, active, selected borders |
| `focus` | `--interactive_focus` | Keyboard focus |

If true alpha works consistently, alpha tokens can replace selected fills. If it does not, use the opaque material tokens exactly as defined in `MASTER.md`.

---

## Qt Stylesheet Compatibility

Qt stylesheets are not modern browser CSS. The theme must assume:

- no reliable `backdrop-filter`,
- no reliable blur,
- limited or inconsistent shadow behavior,
- pseudo-states vary by widget,
- changing border width can shift layout,
- alpha colors may render differently across platforms.

Recommended strategy:

1. Build the theme with opaque colors first.
2. Reserve border width in rest states.
3. Add alpha fills only after visual testing.
4. Use selector-specific focus states instead of a single global assumption.
5. Keep glass effects in tokens, not in one-off component hacks.

---

## Token Mapping Draft

The following block shows the intended variable direction for a future `.obt` theme.

```css
@OBSThemeVars {
    /* Base */
    --bg_window: #070A0F;
    --bg_base: #0D131C;
    --bg_preview: #05070B;

    /* Text */
    --text: #F4F7FB;
    --text_light: #DCE6F3;
    --text_muted: #B9C6D8;
    --text_disabled: #6E7A8D;
    --text_inactive: #DCE6F3;

    /* Glass surfaces */
    --glass_fill_subtle: #121B28;
    --glass_fill: #172235;
    --glass_fill_hover: #1D2A3D;
    --glass_fill_pressed: #223249;
    --glass_edge_soft: #26364B;
    --glass_edge: #344760;
    --glass_edge_strong: #5F789E;
    --glass_sheen: #D9F3FF;

    /* Interaction */
    --primary: #7DD3FC;
    --primary_light: #A5E4FF;
    --primary_lighter: #D9F3FF;
    --primary_dark: #38BDF8;
    --highlight: #7DD3FC;
    --highlight_inactive: #203247;
    --border_highlight: #FDE68A;

    /* Semantic status */
    --warning: #FACC15;
    --danger: #FB7185;
    --success: #4ADE80;
    --record: #F43F5E;
    --streaming: #38BDF8;

    /* Borders */
    --border_color: #26364B;
    --input_border: #344760;
    --input_border_hover: #5F789E;
    --input_border_focus: #FDE68A;

    /* Inputs */
    --input_bg: #0F1722;
    --input_bg_hover: #152033;
    --input_bg_focus: #172235;

    /* Buttons */
    --button_bg: #172235;
    --button_bg_hover: #1D2A3D;
    --button_bg_down: #223249;
    --button_bg_disabled: #111823;
    --button_border: #26364B;
    --button_border_hover: #5F789E;
    --button_border_focus: #FDE68A;

    /* Lists */
    --list_item_bg_hover: #1B2A3F;
    --list_item_bg_selected: #274866;

    /* Tabs */
    --tab_bg: #121B28;
    --tab_bg_hover: #1D2A3D;
    --tab_bg_down: #274866;
    --tab_bg_disabled: #111823;
    --tab_border: #26364B;
    --tab_border_hover: #5F789E;
    --tab_border_focus: #FDE68A;
    --tab_border_selected: #7DD3FC;

    /* Scrollbars */
    --scrollbar: #5F789E;
    --scrollbar_hover: #7DD3FC;
    --scrollbar_down: #38BDF8;
    --scrollbar_border: #1B293A;
}
```

---

## Component Treatment

### Main Window

- Deep background `#070A0F`.
- No bright full-window gradient.
- Preview/canvas well should stay darker than control surfaces.
- Separators should use `--glass_edge_soft` by default and `--glass_edge_strong` on hover.

### Docks

- Dock body uses `--glass_fill_subtle`.
- Dock title uses `--glass_fill` with a bottom border.
- Active title state can use a cyan edge or subtle left accent.
- Dock close and float buttons should be icon-first, transparent at rest, and filled on hover.

### Source And Scene Lists

List states must be unmistakable:

| State | Treatment |
|-------|-----------|
| Rest | Transparent or `--glass_fill_subtle` row |
| Hover | `--list_item_bg_hover` |
| Selected | `--list_item_bg_selected` plus cyan edge or selected icon color |
| Focused selected | selected fill plus `--border_highlight` |
| Disabled | muted text plus no status glow |

Do not rely on blue fill alone for selected state when the row also contains colored status icons.

### Buttons And Toolbuttons

- Use glass fill at rest.
- Hover increases edge strength and fill.
- Pressed deepens the surface.
- Focus uses `--button_border_focus`, not the hover color.
- Destructive buttons use danger only for destructive confirmation or stop/remove actions.

### Inputs And Combo Boxes

- Use dark input wells with visible borders.
- Focus border should be warm amber to avoid confusion with selected/active cyan.
- Placeholder or inactive text must remain readable against the input fill.
- Combo popup selected rows should mirror list selected rows.

### Tabs

- Rest tabs look like low glass plates.
- Selected tabs use active fill plus cyan border.
- Focused tabs add amber border.
- Hover cannot look stronger than selected.

### Sliders And Meters

- Tracks: dark glass well.
- Filled ranges: cyan for normal active values.
- Warning ranges: amber.
- Critical/peaking ranges: red or recording token.
- Handles must meet icon/control contrast and remain visible over active and inactive track segments.

### Menus And Context Menus

- Menu background should use `--surface_overlay` or `--glass_fill`.
- Borders use `--glass_edge`.
- Hovered item uses `--glass_fill_hover`.
- Disabled menu text uses `--text_disabled`.
- Avoid transparent menus if the OS compositor makes text readability inconsistent.

### Dialogs

- Use `--surface_overlay_strong` for modal surfaces.
- Dialogs need stronger border than dock panels.
- Primary action gets accent fill only when there is one clear primary action.
- Destructive actions must not sit next to primary actions without separation.

---

## Accessibility Checklist

Before implementation is accepted:

- Primary text on all window, panel, input, menu, and dialog surfaces passes 4.5:1.
- Secondary text passes 4.5:1 when it communicates status or configuration.
- Focus is visible on keyboard traversal for buttons, tabs, line edits, combo boxes, tree/list rows, sliders, and dock title buttons.
- Hover and focus are visually distinct.
- Selected and inactive-selected rows remain visible.
- Recording/streaming/error states are not color-only where the widget supports icon/text pairing.
- Disabled controls look disabled but remain readable enough to identify.
- No state change causes layout shift.
- Existing OBS font scaling remains respected.

---

## Fallback Rules For No Blur

The no-blur fallback is the default:

| Desired glass effect | Qt-safe fallback |
|----------------------|------------------|
| Frosted panel | Pre-blended opaque surface color |
| Light refraction edge | 1px border with `--glass_edge_soft` |
| Raised glass card | Slightly lighter fill plus stronger border |
| Glow | Small status icon color or border, not large shadow |
| Background blur behind modal | Strong opaque overlay surface plus dark scrim |
| Glass shine | Optional top border using `--glass_sheen` at low opacity, or skip |

If alpha is used, it must not be required for contrast. Test text against the worst-case composited background, not only against the nominal token.

---

## Implementation Sequence Proposal

1. Clone the existing Aitum theme under a new name.
2. Replace only the token block first.
3. Validate OBS startup and theme loading.
4. Verify global surfaces, text, inputs, buttons, tabs, and lists.
5. Add selector-specific focus improvements.
6. Add dock and toolbar refinements.
7. Add optional alpha tokens only where rendering is stable.
8. Capture screenshots for Scenes, Sources, Audio Mixer, Controls, Settings, and Aitum docks.
9. Run a contrast pass on representative screenshots.

---

## Acceptance Criteria

- The theme reads as dark glassmorphism while staying professional and dense.
- It remains usable with all alpha/blur enhancements removed.
- It has AA-compliant text contrast for normal production use.
- Keyboard focus is consistently visible.
- OBS/Aitum status states are faster to scan than in a purely decorative glass theme.
- New installs and first-run setup select `com.obsproject.Aitum.Glass` when the theme is available, with fallback to the original Aitum theme for partial/legacy packages.
