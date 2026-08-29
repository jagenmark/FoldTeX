# FoldTeX

FoldTeX is a small native Qt Quick app for LaTeX notes on Omarchy. The active line stays as source. Press Enter to render it with TeX Live and move to the next line. Click a rendered line or move back to it to edit the source again.

Long source and prose rows wrap to the current writing width. Their height grows
with the wrapped text, including when the window or side margin changes.

It reads the active Omarchy colors from `~/.local/state/omarchy/current/theme/colors.toml` and updates when the theme changes.

Shortcuts:

- `Enter`: render the line and move down
- `Up` or `Down`: move between lines at the start or end of a source line
- `Ctrl+S`: save
- `Ctrl+Shift+S`: save as
- `Ctrl+O`: open
- `Ctrl+Q`: save recovery and quit
- `Ctrl+'`, `Ctrl+,`, or `Ctrl+Shift+F`: choose the writing font, size, and side margin
- `Ctrl+F`: find and replace words in the note
- `Ctrl+G`: open the searchable LaTeX guide in a separate window
- `Ctrl+K`: search common LaTeX by plain words and insert it
- `Ctrl+Shift+R`: cycle between automatic, all-source, and all-rendered views
- `Ctrl+Shift+E`: export the note as TeX or PDF
- `F1`: show all keyboard shortcuts

Shift-click another row, or use Shift+Up and Shift+Down, to select a range of
lines. Copy, cut, and Delete then act on the full range. `Ctrl+Shift+A` selects
all rows. Pasting text that contains line breaks creates a row for each line.
Backspace at the start of a row joins it to the row above; Delete at the end
joins it to the row below.

Find runs as you type against both prose and the LaTeX source behind rendered
equations. The current result has a strong highlight; other results keep a
faint highlight.

The `\\` control opens the same LaTeX finder. Search for terms such as `root`,
`fraction`, `integral`, `matrix`, or `approximately`.

The `?` control opens keyboard help. The `A`, `S`, or `R` control shows the
current view mode. The down arrow opens TeX and PDF export. The `G` control
opens the offline LaTeX guide. Click a guide snippet to copy it. Up and Down
scroll the guide; Page Up and Page Down move by most of a screen. Trackpad
scrolling is faster in both the guide and the document.

The `Aa` control in the title bar opens the same writing settings. FoldTeX remembers the font, size, and side margin. The size changes source, prose, and rendered equations; equations keep the LaTeX math font. The chosen side margin applies at the normal window size and drops toward 16 logical pixels as the window gets smaller.

In the Aa panel, Up and Down move through Font, Size, Side margin, and the
buttons. Left and Right change the chosen setting; at the bottom they switch
between Cancel and OK. Enter applies from any setting, or activates the chosen
bottom button.

Build:

```sh
mkdir -p build
cd build
qmake6 ../foldtex.pro
make -j$(nproc)
```
