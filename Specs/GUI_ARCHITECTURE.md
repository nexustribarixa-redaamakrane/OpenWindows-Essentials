# OpenWindows GUI Architecture

## Component Stack
``nApplications   : owterm, owfm, ownotepad, owpaint, owcalcgui
Widgets        : owmenu64, owdlg64, owicon64, owdnd64, ownotif64
Window Manager : owwm64 + owwm
Rendering      : owgfx64, owfont64, sufrender
Display        : fbdev, vbedrv, bootvid
``n
## Theme System
- Managed by owtheme64
- Default theme: dark mode
- Theme files stored in /etc/themes/

