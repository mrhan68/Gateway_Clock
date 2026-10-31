---
name: Smart Clock & Industrial Edge IoT Gateway
description: High-density dark-mode operations dashboard and real-time edge telemetry supervisor
colors:
  dark-base: "#09090b"
  dark-surface: "#121215"
  dark-hover: "#18181b"
  dark-border: "#27272a"
  dark-muted: "#3f3f46"
  online: "#10b981"
  warning: "#f59e0b"
  offline: "#f43f5e"
  sync-cloud: "#0ea5e9"
  actuator-cyan: "#06b6d4"
  text-primary: "#f4f4f5"
  text-muted: "#a1a1aa"
  text-dim: "#71717a"
typography:
  display:
    fontFamily: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    fontSize: "1.75rem"
    fontWeight: 700
    lineHeight: 1.2
    letterSpacing: "-0.02em"
  heading:
    fontFamily: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    fontSize: "1.125rem"
    fontWeight: 600
    lineHeight: 1.3
    letterSpacing: "-0.01em"
  body:
    fontFamily: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    fontSize: "0.875rem"
    fontWeight: 400
    lineHeight: 1.5
    letterSpacing: "0em"
  mono:
    fontFamily: "'JetBrains Mono', ui-monospace, SFMono-Regular, Menlo, monospace"
    fontSize: "0.75rem"
    fontWeight: 500
    lineHeight: 1.4
    letterSpacing: "0.02em"
rounded:
  sm: "6px"
  md: "8px"
  lg: "12px"
  xl: "16px"
spacing:
  xs: "4px"
  sm: "8px"
  md: "16px"
  lg: "24px"
  xl: "32px"
components:
  card:
    backgroundColor: "{colors.dark-surface}"
    textColor: "{colors.text-primary}"
    rounded: "{rounded.lg}"
    padding: "16px"
  badge-online:
    backgroundColor: "rgba(16, 185, 129, 0.1)"
    textColor: "{colors.online}"
    rounded: "{rounded.sm}"
    padding: "2px 8px"
  button-primary:
    backgroundColor: "{colors.online}"
    textColor: "{colors.dark-base}"
    rounded: "{rounded.md}"
    padding: "8px 16px"
---

# Design System

## Overview
A technical, high-aesthetic dark-mode interface built for continuous industrial and IoT edge monitoring. Designed to operate 24/7 on maintenance screens, wall tablets, and control consoles without visual fatigue. Emphasizes instant legibility, unambiguous hardware states, and tactile responsiveness.

## Colors
- **Surfaces**: Jet black base (`#09090b`), elevated card panels (`#121215`), and active hover surfaces (`#18181b`).
- **Borders & Dividers**: Subtle hairline dividers (`#27272a` / `border-zinc-800`), avoiding stark white borders.
- **Status LED Accents**:
  - `Online / Active / Normal`: Emerald (`#10b981`).
  - `Warning / Threshold Tripped / Pending`: Amber (`#f59e0b`).
  - `Offline / Error / Cutoff`: Rose (`#f43f5e`).
  - `Cloud / MQTT Sync / BLE Streaming`: Sky (`#0ea5e9`).
  - `Actuators / Relay Triggers`: Cyan (`#06b6d4`).

## Typography
- **Primary Interface**: Clean native system font stack (`system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto`) with clear optical scaling from 10px status micro-labels to 24px clock displays.
- **Telemetry & Numbers**: Dedicated JetBrains Mono / tabular numerals (`tabular-nums`) for stable, jitter-free reading of live metrics, timestamps, and clock counters.

## Layout
- **Density Scale**: 4px base mathematical rhythm (4px, 8px, 12px, 16px, 24px, 32px).
- **Scanability**: Key Performance Indicators (KPIs) in tight, balanced 2-4 column grids with immediate visual status badges.
- **Hierarchy**: Primary task and relay actuation controls are visible on the first viewport without vertical hunt.

## Elevation & Depth
- **Depth Paradigm**: Flat low-saturation surfaces with single-token 1px borders (`border-zinc-800/80`).
- **Glow Accents**: Micro status lights carry soft radial halos (`shadow-[0_0_6px_rgba(52,211,153,0.8)]`) to signify real electrical power and live connection.

## Shapes
- **Container Radii**: Cards and dialogs use 12px (`rounded-xl`), interactive inputs use 8px (`rounded-lg`), status badges use pill capsules (`rounded-full`).

## Components
- **Tactile Toggle**: Smooth physics sliding switch for hardware relays (Green = ON, Zinc = OFF) with disabled state during inflight ACK.
- **Telemetry Chart**: Canvas/SVG polyline with low-opacity vertical area gradients (`rgba(6, 182, 212, 0.15)`), dynamic min/max bounds, and responsive grid ticks.
- **Audit Event Row**: Monospace timestamp, status dot, and device ID pill with high contrast.

## Do's and Don'ts
### Do
- Always use `tabular-nums` for clocks, sensor readings, and counters.
- Provide explicit loading and disabled states for all network and relay actuation triggers.
- Keep background dark-mode palette grounded in zinc (`zinc-950`, `zinc-900`, `zinc-800`).
- Use accessible focus-visible rings (`focus-visible:ring-2 focus-visible:ring-emerald-500/50`).

### Don't
- Never use generic purple-to-blue AI gradients or violet headings.
- Never use gray text on colored warning banners (use tinted dark or near-white text for contrast).
- Never allow full page reloads for internal navigation; use React Router `<Link>`.
- Do not use decorative bouncing animations; stick to subtle opacity and slide transitions.
