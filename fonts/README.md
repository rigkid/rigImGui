# rigImGui fonts

**Stay in `rigImGui`** so author UI looks good out of the box.

| Asset | License | Role |
|-------|---------|------|
| `Roboto-Regular.ttf` | Apache 2.0 (Google) | Default UI body font |
| `fa-solid-900.ttf` | Font Awesome Free 5 | IconFont merge |
| `fontRobotoRegular.h` | - | Tiny stub fallback only |

## Runtime layout (important)

Paths are **exe-relative**, not cwd:

```
stack_install_tool.exe
data/
  fonts/
    Roboto-Regular.ttf
    fa-solid-900.ttf
```

CMake `POST_BUILD` copies these into `$<TARGET_FILE_DIR>/data/fonts`.  
`AppPaths::getFontsDir()` returns `<exeDir>/data/fonts` after `AppPaths::init(argc, argv)` (called from `RigKitEngine`).

The folder sits next to the app binary (`<exeDir>/data/...`), not relative to cwd.
