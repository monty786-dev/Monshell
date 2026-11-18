# Taskbar & Desktop by Monty

Custom taskbar, start panel, window title bars, reveal bar and desktop for **Winlator's Wine** (Box64 + Wine on Android).
Written in plain C (Win32 / GDI), no runtime dependencies.

If you like it, **give it a star on GitHub**: https://github.com/monty786-dev/Winlator-Wine-Look-changer

> **Status:** the code is verified to compile and link against the real mingw-w64 headers/import tables (32- and 64-bit),
> but has not been run on a real Winlator device yet. Expect some visual/behaviour bugs - please open issues with details.

## Get setup.exe from GitHub (no PC toolchain needed)
1. Push this repo to GitHub (keep the folder layout: `src/`, `installer/`, `.github/workflows/build.yml`).
2. Open **Actions > Build setup.exe**; it runs on every push (or press *Run workflow*).
3. Download the artifact: `MontyShell-x86_64` (64-bit container) or `MontyShell-i686` (32-bit container).
   Each contains `MontyShell-Setup-<arch>.exe` (the installer) and `MontyShell-<arch>.exe` (the plain program).
4. Tag a release (`git tag v1.0.0 && git push --tags`) to attach the installers to a GitHub Release.

Local build: `./build.sh` (64-bit) or `./build.sh i686` (needs `mingw-w64`).

## Install in Winlator
Copy the setup to the container's drive, run it from Winlator, tick what you want:
* **Autostart**: adds `HKCU\...\Run\MontyShell` (`--boot`, honours *Startup delay*).
* **Default shell**: replaces `wfm.exe` (looked up in `C:\windows` and `system32`), keeping `wfm.exe.orig`; *Uninstall* restores it.
* **Wine tweaks**: no native frame, no focus fights, faster menus, font smoothing, shorter hang timeouts, no crash dialog.
* Start-menu / desktop shortcuts.

## What is in it
| Area | Features |
|---|---|
| Themes | 23 looks (None, Win 95/98/XP/Vista/7/8/10/11, macOS, Ubuntu, Arch, Mint, XFCE, Cinnamon, KDE, Zorin, flowOS, elementary, Pop!_OS, Deepin, Fedora, ChromeOS) each with **Modern** and **Classic original** mode |
| Taskbar | position (4 edges), z-order, alignment (right = start button on right corner), height, auto-width, icon size, titles, rounding, detach, colour, underline (adapts to icon colour), divider, pinned + running buttons, live hover preview, clock (+weekday), show-desktop, battery / Wi-Fi / Bluetooth / task-manager icons, tray apps (inline or grid popup) |
| Full-screen | keep / over app / floating icon / hide, for full-screen, maximized or both |
| Reveal bar | colour, position, alignment, L-corner, opacity, height, width, visible-seconds, 6 animations, per-app exceptions |
| Start panel | 4 sizes, profile picture + name, search, All / Favourites / Recent, "+" menu (programs, scan installed, system apps, control panel), draggable + resizable tiles, power / settings / run / programs / system / collapse bar, bar position, backgrounds (colour / picture / transparent), transparency |
| Title bars | overlay kept directly above each window's native bar; 22 styles + custom colour; min / max / close / pin-on-top / roll-up; transparency |
| Personalize | effects (glass, iOS blur, 3D, boxed, liquid), accent, shine, gloss, rounding, per-surface effect toggles, 16 icon presets, 7 colour modes, icon recolour, font, size, UI scale |
| Desktop mode | icon grid, corners, lock, wallpaper (png/jpg/bmp/gif) + fit modes, clock widget |
| Settings | everything auto-saved to `%APPDATA%\MontyShell\settings.ini`; backup / restore / reset |
| Efficiency | repaint only on change, 2x slower polling on battery, preview bitmaps freed when hidden, GDI objects cached |

## Not included (and why)
* **Gboard / Android keyboard auto-open**: not possible. Wine runs inside Winlator's Linux container and has no channel to call Android APIs;
  Winlator exposes no such hook. Use Winlator's own keyboard button.
* Tile slide animation, drop shadows, free (non-grid) icon placement: not implemented yet.
* Bluetooth / Wi-Fi icons are indicators: Wine cannot see or control Android radios (Wi-Fi shows "network up").
* Window *borders* are not repainted; only the title strip is replaced (accent border optional).

## Layout
`src/` settings (X-macro table), themes, drawing toolkit, taskbar, preview, start panel, reveal, title bars, desktop, settings UI, shell helpers.
`installer/setup.c` GUI installer/uninstaller (embeds the shell). `build.sh` local build. `.github/workflows/build.yml` CI.
