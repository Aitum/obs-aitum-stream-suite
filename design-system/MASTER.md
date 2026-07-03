# Design System - OBS Aitum Stream Suite Glass

**Project**: OBS Aitum Stream Suite
**Theme concept**: Dark professional glassmorphism for streaming and productivity workflows
**Primary target**: OBS Studio desktop UI with Aitum docks, dialogs, source lists, output controls, and settings panels
**Implementation target**: Qt stylesheet theme tokens, with no requirement for real backdrop blur

---

## Product Positioning

| Attribute | Direction |
|-----------|-----------|
| Product type | Streaming operations tool / creator productivity suite |
| Primary users | Streamers, technical directors, creators, and operators who need fast status scanning |
| Core mental model | Control room dashboard: live state, scenes, sources, output health, and configuration |
| Visual tone | Dark, calm, precise, premium, content-dense |
| Style reference | Glassmorphism used as a surface language, not as decorative haze |

The theme should make live production feel controlled and legible. It can look modern and polished, but it must not sacrifice contrast, focus visibility, dense scanning, or OBS-native predictability.

---

## Core Principles

1. **Legibility before translucency**
   Every primary label, value, and control state must meet WCAG AA contrast. Glass surfaces are simulated with opaque blended colors when Qt cannot provide reliable blur.

2. **Professional glass, not neon glass**
   Use layered dark surfaces, restrained cyan/teal highlights, fine borders, and small status glows. Avoid large blurry gradients, heavy purple dominance, and decorative light blobs.

3. **Visible focus is mandatory**
   Keyboard focus must be clearly visible on inputs, buttons, tabs, lists, and dock controls. Focus cannot rely on color alone when possible: combine border color, surface change, and reserved focus width.

4. **Status color must be semantic**
   Recording, streaming, errors, warnings, success, disabled, and inactive states need named tokens. Do not encode meaning with hue alone; pair color with icon, text, or state shape when UI allows it.

5. **Stable dense controls**
   Hover, pressed, selected, and focus states must not resize controls. Reserve border widths in resting states so focus borders do not cause layout shift.

6. **Qt-compatible fallback first**
   Treat blur as optional. The base theme should work with opaque colors, alpha fills, borders, and separators. If blur is available, it is a progressive enhancement.

---

## Style Direction

### Glassformismo Dark Control Room

This theme uses glassmorphism as a layered desktop material system:

- Backgrounds are near-black blue-green charcoal, not pure black.
- Main panels use high-opacity glass tints that look translucent but remain readable.
- Borders carry the glass edge effect.
- Selection and focus use crisp luminous accents.
- Status signals use controlled color and compact indicators.
- Shadows are subtle and mostly simulated through color separation, because Qt stylesheets do not consistently support modern CSS shadow behavior.

### Anti-Patterns

| Use | Avoid |
|-----|-------|
| Dark neutral surfaces with blue-green undertones | Large purple/blue gradients across the whole UI |
| 1px glass edge borders plus strong focus tokens | Low-contrast gray borders that disappear on dark backgrounds |
| Opaque fallback colors that mimic glass | Depending on `backdrop-filter` or real blur for readability |
| Compact, stable, OBS-native control geometry | Oversized marketing UI, pill-heavy controls, or decorative card stacks |
| One consistent icon style from existing theme assets | Emoji or mixed icon weights |

---

## Token Architecture

Tokens should be layered in this order:

1. Raw palette tokens: fixed color values.
2. Semantic tokens: text, surface, border, state, status.
3. Component tokens: button, input, dock, tab, list, scrollbar.
4. OBS compatibility aliases: map semantic tokens to existing OBS theme variables where needed.

Do not hardcode component colors outside the token block unless a Qt selector requires a local override.

---

## Color System

### Raw Palette

```css
/* Base */
--glass_ink_0: #070A0F;
--glass_ink_1: #0B1018;
--glass_ink_2: #101824;
--glass_ink_3: #162233;
--glass_ink_4: #1D2B3F;

/* Glass fallback surfaces: pre-blended opaque colors */
--glass_surface_0: #0D131C;
--glass_surface_1: #121B28;
--glass_surface_2: #172235;
--glass_surface_3: #1D2A3D;
--glass_surface_raised: #223249;

/* Text */
--glass_text_0: #F4F7FB;
--glass_text_1: #DCE6F3;
--glass_text_2: #B9C6D8;
--glass_text_3: #8F9DB0;
--glass_text_disabled: #6E7A8D;

/* Accents */
--glass_cyan: #7DD3FC;
--glass_teal: #5EEAD4;
--glass_blue: #93C5FD;
--glass_lavender: #C4B5FD;
--glass_focus: #FDE68A;

/* Status */
--glass_success: #4ADE80;
--glass_warning: #FACC15;
--glass_danger: #FB7185;
--glass_recording: #F43F5E;
--glass_streaming: #38BDF8;
--glass_inactive: #94A3B8;
```

### Semantic Tokens

```css
/* App surfaces */
--bg_window: var(--glass_ink_0);
--bg_base: var(--glass_surface_0);
--bg_preview: #05070B;
--surface_panel: var(--glass_surface_1);
--surface_panel_hover: var(--glass_surface_2);
--surface_panel_active: var(--glass_surface_3);
--surface_overlay: #172235;
--surface_overlay_strong: #1D2A3D;

/* Glass material tokens */
--glass_fill: #172235;
--glass_fill_subtle: #121B28;
--glass_fill_hover: #1D2A3D;
--glass_fill_pressed: #223249;
--glass_edge: #344760;
--glass_edge_soft: #26364B;
--glass_edge_strong: #5F789E;
--glass_sheen: #D9F3FF;

/* Text */
--text: var(--glass_text_0);
--text_light: var(--glass_text_1);
--text_muted: var(--glass_text_2);
--text_subtle: var(--glass_text_3);
--text_disabled: var(--glass_text_disabled);
--text_on_accent: #061018;

/* Interaction */
--interactive_primary: var(--glass_cyan);
--interactive_primary_hover: #A5E4FF;
--interactive_primary_pressed: #38BDF8;
--interactive_secondary: var(--glass_lavender);
--interactive_focus: var(--glass_focus);
--interactive_selected: #274866;
--interactive_selected_border: var(--glass_cyan);

/* Status */
--status_success: var(--glass_success);
--status_warning: var(--glass_warning);
--status_danger: var(--glass_danger);
--status_recording: var(--glass_recording);
--status_streaming: var(--glass_streaming);
--status_inactive: var(--glass_inactive);
```

### Optional Alpha Tokens

Use these only when the target Qt stylesheet path renders alpha reliably. The opaque tokens above remain the canonical fallback.

```css
--glass_alpha_panel: rgba(23, 34, 53, 0.88);
--glass_alpha_panel_hover: rgba(29, 42, 61, 0.92);
--glass_alpha_overlay: rgba(13, 19, 28, 0.94);
--glass_alpha_border: rgba(148, 197, 253, 0.32);
--glass_alpha_sheen: rgba(217, 243, 255, 0.10);
```

---

## Contrast Requirements

Minimum contrast targets:

| Pair | Target |
|------|--------|
| Primary text on app/window/surface | 4.5:1 or higher |
| Secondary text on app/window/surface | 4.5:1 for normal text |
| Muted metadata | 3:1 minimum, 4.5:1 when it carries workflow-critical meaning |
| Icon-only controls | 3:1 minimum against adjacent surface |
| Focus ring | Visibly distinct from rest, hover, and selected states |
| Error/warning/success labels | 4.5:1 when text is colored directly |

Reference color pairs designed to pass AA:

| Foreground | Background | Usage |
|------------|------------|-------|
| `--glass_text_0` `#F4F7FB` | `--glass_ink_0` `#070A0F` | Main text |
| `--glass_text_1` `#DCE6F3` | `--glass_surface_1` `#121B28` | Panel text |
| `--glass_text_2` `#B9C6D8` | `--glass_surface_1` `#121B28` | Secondary metadata |
| `--glass_cyan` `#7DD3FC` | `--glass_surface_0` `#0D131C` | Links, active icons |
| `--glass_focus` `#FDE68A` | `--glass_surface_2` `#172235` | Focus border |

---

## Typography

OBS and Qt should continue using the app/system font pipeline. The theme should not require new font assets.

| Role | Token direction | Usage |
|------|-----------------|-------|
| Base | Existing `--font_base` | General UI text, forms, dock content |
| Small | Existing `--font_small` | Metadata, secondary labels, captions |
| Large | Existing `--font_large` | Dialog headings, section titles |
| Numeric | Tabular figures when available | Timers, counters, bitrate, dropped frames, output status |

Typography rules:

- Preserve OBS font scaling variables.
- Avoid body text below the existing small token unless the component is purely decorative.
- Use weight and color sparingly; dense controls should rely on spacing, separators, and state tokens.
- Long source names, scene names, and output labels should truncate with tooltip access where code supports it.

---

## Layout And Density

Keep the current OBS density model and refine visual hierarchy through surfaces:

| Token | Direction |
|-------|-----------|
| `--border_radius_small` | 3px |
| `--border_radius` | 5px |
| `--border_radius_large` | 7px |
| `--highlight_width` | 1px minimum, 2px where geometry is reserved |
| `--scrollbar_size` | 10px desktop default |

Spacing should stay compact and predictable:

- Docks and lists remain content-dense.
- Dialogs can breathe slightly more than dock panels.
- Toolbars use compact icon buttons with clear hover and pressed states.
- Lists and trees must retain stable row height across hover, selected, disabled, and focus states.

---

## Component Direction

### App Window And Main Background

- Use `--bg_window` for the deepest application canvas.
- Use `--bg_base` or `--surface_panel` for central work areas.
- Use `--bg_preview` for preview/canvas areas where the content should dominate.
- Avoid bright background gradients behind dense controls.

### Docks

- Dock body: `--glass_fill_subtle`.
- Dock title: slightly raised `--glass_fill` with `--glass_edge_soft`.
- Active/focused dock: stronger title border using `--glass_edge_strong`.
- Dock close/float buttons: transparent at rest, `--glass_fill_hover` on hover, `--glass_fill_pressed` on pressed.

### Buttons

```css
--button_bg: var(--glass_fill);
--button_bg_hover: var(--glass_fill_hover);
--button_bg_down: var(--glass_fill_pressed);
--button_bg_disabled: #111823;
--button_border: var(--glass_edge_soft);
--button_border_hover: var(--glass_edge_strong);
--button_border_focus: var(--interactive_focus);
```

Primary actions should use accent fills only for the most important action in a panel or dialog. Secondary buttons should stay glass-surface based.

### Inputs

```css
--input_bg: #0F1722;
--input_bg_hover: #152033;
--input_bg_focus: #172235;
--input_border: #344760;
--input_border_hover: #5F789E;
--input_border_focus: var(--interactive_focus);
```

Inputs need a strong focus border and must keep readable text contrast in placeholder, selected text, disabled, and validation states.

### Lists And Trees

```css
--list_item_bg_hover: #1B2A3F;
--list_item_bg_selected: #274866;
--list_item_border_selected: var(--glass_cyan);
--highlight: var(--glass_cyan);
--highlight_inactive: #203247;
```

Selected rows should read as selected even when the window loses focus. Inactive selection can be less saturated but must remain visible.

### Tabs

Tabs should feel like segmented glass sheets:

- Rest: `--glass_fill_subtle`.
- Hover: `--glass_fill_hover`.
- Selected: `--interactive_selected`.
- Selected border: `--interactive_selected_border`.
- Focus: `--interactive_focus`.

### Scrollbars

Scrollbars should be visible but quiet:

```css
--scrollbar: #5F789E;
--scrollbar_hover: #7DD3FC;
--scrollbar_down: #38BDF8;
--scrollbar_border: #1B293A;
```

### Recording And Streaming States

- Recording: `--status_recording`, never only text color; include icon state or filled dot where available.
- Streaming active: `--status_streaming`.
- Connected/healthy: `--status_success`.
- Warning: `--status_warning`.
- Error/disconnected: `--status_danger`.

Status colors should be compact accents, not full-panel floods, except for destructive confirmation states.

---

## Focus And Keyboard Accessibility

Focus styling requirements:

- Every interactive widget must have a visible `:focus` state.
- Focus must be distinct from hover and selected.
- Resting controls should reserve border width so focus does not shift layout.
- For controls where Qt cannot draw an outer ring, use a bright border plus darker inner fill.
- Do not remove native keyboard traversal indicators without replacement.

Recommended focus mapping:

```css
--focus_ring: var(--interactive_focus);
--focus_ring_soft: #8A7D48;
--focus_fill: #1B2A3F;
--focus_text: var(--glass_text_0);
```

---

## Motion And Feedback

Qt stylesheets offer limited animation support, so the theme should rely on immediate but clear state changes:

- Hover: small surface lift via color, not geometry.
- Pressed: darker/denser fill.
- Disabled: lower text contrast plus muted border.
- Loading/progress: use existing OBS progress affordances and semantic colors.
- Avoid animation requirements in the theme spec unless implemented by existing OBS widgets.

---

## Implementation Guardrails

- Do not require `backdrop-filter`, blur shaders, CSS filters, or custom painting for basic readability.
- Alpha colors may be used as enhancement only after testing on Windows, macOS, and Linux.
- Keep all meaningful states tokenized.
- Keep icon assets from the existing theme unless a separate icon pass is approved.
- Keep theme assets outside `resources.qrc`; package installation may update build helpers so the theme is installed on each supported platform.
- Validate the final theme visually in dock-heavy layouts: Scenes, Sources, Audio Mixer, Controls, Output, Settings, and Aitum-specific docks.

---

## Related Page Specs

- `design-system/pages/theme-glassmorphism.md` - detailed implementation proposal for the complete glassmorphism OBS/Aitum theme.
