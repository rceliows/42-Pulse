# Custom Major Module — Godot Interactive Globe & UI Shell

## Why this module
- The core experience is a 3D, interactive globe showcasing 42 campuses, presence, and performance; Godot is the primary UI, not just a visual garnish.
- The app embeds Godot as the main client, handling navigation, camera, and rich interaction while integrating with our API for live data (presence, leaderboards, profiles).
- This goes beyond a typical web canvas: it is the primary UX layer, with stateful UI, controls, and data binding to the backend.

## Technical scope and challenges
- Godot web export (HTML/JS/wasm) integration into a responsive shell.
- Globe rendering: tiled/optimized meshes, halos, nearest-neighbor links, animated markers, and performance tuning for web builds.
- Camera and interaction: mouse/touch/trackpad navigation, smooth zoom/pan/orbit, hit-testing on campus markers, accessible controls/tooltips.
- Data integration: GDScript bridges to REST/WebSocket endpoints for presence, stats, chat indicators; error handling and reconnection logic.
- UI inside Godot: panels for campus details, user profiles, filters; buttons bound to API calls; lightweight state management.
- Performance: asset loading, caching, LOD strategies to keep FPS stable in the browser build.

## Value to the project
- Delivers the signature visualization (42 campuses worldwide) with interactive, data-driven overlays.
- Provides a cohesive UI without relying on a conventional web framework, aligned with the project’s “freedom” direction.
- Differentiates the project with a rich, game-like experience while still integrating real data and live updates.

## Why it deserves Major status (2 pts)
- Substantial engineering: 3D rendering, input handling, stateful UI, and live data integration all inside Godot, delivered via web export.
- Covers multiple layers: rendering, interaction, networking, and performance tuning for the browser target.
- Serves as the primary user interface, not an auxiliary widget, making it central to the app’s functionality.

## Fit with the rest of the system
- Backend: FastAPI provides REST/WebSocket endpoints consumed by Godot scripts.
- Shell: a thin HTML/Tailwind wrapper embeds the Godot build for responsive layout and legal/accessibility requirements.
- Analytics: globe links to dashboards (export/filters) and can surface live metrics overlays.
