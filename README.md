# dwm

My daily window manager on Void Linux. Based on dwm 6.3 and the terror-dwm fork, with a bottom bar, per-tag layouts and a monochrome palette.

![dwm desktop](assets/desktop.png)

## Improvements

- Per-tag layout state, a scratchpad terminal, hide/show and clickable window tabs.
- Status2d drawing and status text on every monitor; clickable battery and clock areas.
- Stable tray ordering, including native XEmbed and bridged StatusNotifier icons.
- Bar labels use application names instead of document or terminal titles.
- Automatic terminal swallowing on Linux, including terminals sharing urxvtd.
- Tests cover bar-label privacy and 720 tray docking permutations. Separate X11 tests exercise pointer grabs and tray stacking.

## Build and use

Requires a C compiler, make, Xlib, Xft, Xinerama and XRes development headers. On Void:

```sh
sudo xbps-install -S base-devel libX11-devel libXft-devel libXinerama-devel libXres-devel
make
make test
sudo make install
```

Read `config.h` before compiling. `config.def.h` contains the same defaults. Add `exec dwm` to the end of your X session startup file. The companion [Void configuration](https://github.com/merthium/void-config) includes a small `.xinitrc` and slstatus.

`Alt` is the modifier. `Alt+a` opens a terminal, `Alt+p` opens dmenu, `Alt+j/k` changes focus, `Alt+1–9` changes tags, `Alt+h` hides a window, `Alt+Shift+s` restores hidden windows, and `Alt+grave` toggles the scratchpad. `Alt+Shift+q` exits.

Optional commands: urxvt, dmenu, mc, htop, zathura, xdg-open, pavucontrol, pactl, brightnessctl, flameshot, networkmanager_dmenu, upower and cal. Adjust the launchers and battery device for your machine. Personal helper commands have been replaced with standard programs. Fonts: Fragment Mono and Lilex Nerd Font Mono; change `fonts` if needed.

The X11 tests must run on a separate X server with `DWM_ISOLATED_X_TEST=1`; they are not part of `make test`.

## Terminal swallowing

On Linux, a GUI application launched from a terminal automatically takes its
tile. Closing the application restores the terminal. For example, run
`feh image.png` or `nsxiv image.png` in urxvt. No wrapper or shell hook is needed.

`swallowautomatic` enables this behavior. `terminalapps` lists lowercase
WM_CLASS names. The application or one of its ancestors must inherit `WINDOWID`,
and its local PID must be available through XRes. dwm reads `/proc/PID/environ`
and follows `PPid` in `/proc/PID/status` through up to 64 ancestors, so programs
that clear their environment can still use the launching shell's terminal ID. Using the
window ID rather than the terminal's PID keeps urxvtc windows distinct even
when they share one urxvtd process. New terminal windows and transient dialogs
are excluded; an already swallowing terminal does not swallow another window.
Programs that reuse an existing process, or lose both their terminal ancestry
and `WINDOWID`, may open separately. The existing `_BETTER_SWALLOW` registration interface remains
available for explicit swallowing.

`tests/swallow` checks real XRes PID lookup, two terminals sharing a process,
replacement tile geometry, and terminal restoration on an isolated X server.
The `--sanitized-test` case verifies a GUI process that drops `WINDOWID` while
its launcher retains the terminal identity.

## Credits

Based on [suckless dwm](https://dwm.suckless.org/) and [dlmayhem/terror-dwm](https://codeberg.org/dlmayhem/terror-dwm), identified in the original README. Patch ideas include pertag, systray, awesomebar, status2d, statusallmons, push and uselessgap. The integrated source is the build; the retained statusallmons diff is a reference, not an extra installation step.

The grayscale palette is inspired by [eyes.nvim](https://github.com/amedoeyes/eyes.nvim). Original MIT/X Consortium notices are in `mit-x-consortium.txt`; the inherited fork is GPL-3.0-or-later, reproduced in `LICENSE` and `gpl-v3.txt`.
