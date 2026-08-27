# Color schemes

Named palettes are JSON files, not C++ enum values. Dark and Light stay as the
only built-in bases; a scheme overlays colors and a few metrics on top.

Search order (later folders lose to an earlier filename match):

1. `<userData>/user/themes` — user saves and overrides
2. `<exeDir>/data/themes` — shipped catalog (host copies this pack's `themes/`)
3. `packs/rigImGui/themes` — source-tree fallback while developing

| File | Credit | License |
|------|--------|---------|
| `dracula.json` | Dracula palette, Zeno Rocha / [draculatheme.com](https://draculatheme.com) — ImGui mapping by RigKit | MIT (Dracula) |
| `enemy-mouse.json` | enemymouse gist via [dear-imgui-styles](https://github.com/GraphicsProgramming/dear-imgui-styles) | see that repo |
