# Miller Columns regression checks

Build with `meson setup build -Dtests=true`, then `meson compile -C build`.
Run the built `build/thunar/thunar` in a separate desktop/session with temporary
XDG settings so the checks do not change your usual Thunar configuration.
Use `G_DEBUG=fatal-criticals` to catch invalid object operations.

Prepare a temporary directory containing two nested folders, a second sibling
folder, and several regular files. Use only temporary files for these checks.

- **Selection and view changes:** Select several files in List view. Switch
  between Icon, Compact, List, and Miller Columns (`Ctrl+1` through `Ctrl+4`).
  Selection transfer must not crash or produce GLib criticals. Repeat with an
  empty selection and with image preview enabled.
- **Search cancellation:** From Miller Columns, start recursive search with
  `Ctrl+F`, enter a filename, and cancel search. Miller Columns must return
  without a crash. Repeat with and without selected search results.
- **Terminal recovery (VTE builds):** Show the terminal with `F4` in List view,
  switch to Miller, navigate to a different directory, then return to List.
  `F4` must still toggle the terminal, and `pwd` must reflect the displayed
  directory. Repeat in a tab that was originally opened in Miller Columns.
- **Refresh:** Open several nested columns and select a regular file. Press
  `F5`. The columns, widths, and selection must remain. Open the same folder in
  another tab and refresh again; metadata and contents must be reloaded even
  while the other tab holds the folder in its cache.
- **Failed load and retry:** Remove read/search permission from a temporary
  folder and open it in Miller Columns as an unprivileged user. An error dialog
  must identify the failed directory. Close the dialog, restore permissions,
  add a file, and press `F5`; the file must appear. Restore permissions before
  deleting the fixture. Remote enumeration failures should likewise report
  an error and allow refresh after reconnecting.
- **Keyboard search:** With two tabs open, type a folder name in Miller Columns.
  While type-ahead search is active, `Tab` and `Shift+Tab` traverse columns;
  `Ctrl+Tab` and `Ctrl+Shift+Tab` switch tabs. Repeat after search has closed.

Run the automated suite after the checks: `meson test -C build --print-errorlogs`.
The existing symlink test does not cover the GTK interaction checks above.
