# FoldTeX

FoldTeX is a small Qt Quick app for writing LaTeX notes. Each row stays editable
as source until you press Enter, then FoldTeX renders its math and moves to the
next row. Click a rendered row, or move back to it, to edit it again. Plain text
stays as text, and long rows wrap to the current writing width.

FoldTeX currently targets Linux. It is built and tested on Omarchy, which is
based on Arch Linux. There is no package yet. Ubuntu and Debian may work, but
they have not been tested.

## Features

- Save and open `.foldtex` notes; export them as `.tex` or `.pdf`.
- Switch between automatic, all-source, and all-rendered views.
- Find and replace text in both prose and the source behind rendered math.
- Search common LaTeX by plain words, use the offline guide, or type `sqrt`,
  `root`, `frac`, `sum`, `prod`, `int`, `lim`, `cases`, or `mat2` and press Tab.
- Mark rows as definitions, theorems, proofs, or examples, and add timed
  catch-up marks.
- Store course and lecture details and search all `.foldtex` notes in the saved
  note's folder.
- Paste a clipboard image into a saved note. FoldTeX stores it next to the note
  in a matching `.assets` folder.
- Undo and redo changes across the whole note. FoldTeX also saves recovery data
  as you type and keeps up to 30 timed snapshots.
- Split pasted multi-line text into rows, or join rows with Backspace and Delete
  at row bounds.
- Set the writing font, size, and side margin. The margin shrinks on narrow
  windows, and both source and rendered math track the chosen size.
- Draw figures with a pen, eraser, lines, arrows, boxes, and text, then insert
  them as image rows.
- Copy a lecture PDF beside a saved note and view it next to the note. Link a
  row to a slide or capture the visible slide area as an image row.

## Shortcuts

- `Enter`: render the row and move down
- `Up` or `Down`: move within and between rows while editing; scroll the page
  when no row is being edited
- `Shift+click` or `Shift+Up` / `Shift+Down`: select a range of rows
- `Ctrl+Shift+A`: select all rows; copy, cut, Backspace, and Delete then act on
  the whole selection
- `Ctrl+N`: start a new note
- `Ctrl+S`, `Ctrl+Shift+S`, `Ctrl+O`: save, save as, and open
- `Ctrl+Q`: save recovery data and quit
- `Ctrl+Z`, `Ctrl+Shift+Z`: undo and redo
- `Tab`, `Shift+Tab`: move forward and back through snippet fields
- `Ctrl+F`: find and replace in the note
- `Ctrl+K`: find and insert LaTeX by name
- `Ctrl+G`: open the searchable LaTeX guide
- `Ctrl+.`: set the current row type; choose with Up/Down and Enter
- `Ctrl+M`: add a timed catch-up mark
- `Ctrl+Alt+L`: set course and lecture details
- `Ctrl+Alt+F`: search notes in the current note's folder
- `Ctrl+Shift+V`: paste a clipboard image into a saved note
- `Ctrl+Alt+I`: draw a figure
- `Ctrl+Alt+P`: add, show, or hide lecture slides
- `Ctrl+Shift+R`: cycle through automatic, source, and rendered views
- `Ctrl+Shift+E`: export as TeX or PDF
- `Ctrl+'`, `Ctrl+,`, or `Ctrl+Shift+F`: set the font, size, and side margin
- `F1`: show keyboard help

The right-edge controls open the same settings, help, LaTeX finder, view,
export, and guide tools. The guide also supports Up, Down, Page Up, Page Down,
and Escape.

## Figures and lecture slides

The figure tool saves drawings beside the note in its `.assets` folder. It has
a pen, eraser, straight line, arrow, box, text, undo, and clear.

Lecture PDFs open beside the note when the window is wide. On a small window,
FoldTeX switches between the note and slides. It copies an added PDF into
`.foldtex-assets` beside the saved note, so moving the original does not break
the note. The slide tools can link the active row to the current page or capture
the visible slide area as an image row.

## Dependencies

The app needs a C++17 compiler, Make, Qt 6 Core/GUI/Widgets/QML/Quick/Quick
Controls/Quick Dialogs, `qmake6`, `latex`, `pdflatex`, and `dvisvgm`. Its TeX
input uses the `standalone`, `amsmath`, `amssymb`, `mathtools`, `xcolor`, and
`geometry` packages. Qt Test is also needed to build the checks.

On Arch Linux or Omarchy, install the current repo packages with:

```sh
sudo pacman -S --needed base-devel qt6-base qt6-declarative texlive-latexextra dvisvgm
```

This set is tested on Omarchy. `qt6-base` supplies `qmake6`, and
`texlive-latexextra` pulls in the other TeX Live sets used by FoldTeX.

For Ubuntu 24.04 LTS, these package names are present in the Ubuntu repos:

```sh
sudo apt update
sudo apt install build-essential qt6-base-dev qt6-declarative-dev texlive-latex-extra dvisvgm
```

Ubuntu places the Qt and TeX packages in `universe`; enable that repo first if
APT cannot find them. This is an install example, not a claim of Ubuntu test or
package support. The same package names are available on current Debian, but
FoldTeX has not been tested there either.

### Omarchy theme integration

On Omarchy, FoldTeX reads
`~/.local/state/omarchy/current/theme/colors.toml` and updates when the active
theme changes. This is optional: if the file does not exist, FoldTeX uses its
built-in dark colors. Other Linux desktops do not need Omarchy files.

## Build and run

From the repository root:

```sh
mkdir -p build/app
cd build/app
qmake6 ../../foldtex.pro
make -j"$(nproc)"
./foldtex
```

## Tests

From the repository root, build and run both checks with:

```sh
mkdir -p build/tests/backend build/tests/keyboard

cd build/tests/backend
qmake6 ../../../tests/tests.pro
make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= \
  ./backend_regression ../../../tests/fixtures/multiline.foldtex

cd ../keyboard
qmake6 ../../../tests/keyboard_tests.pro
make -j"$(nproc)"
./keyboard_regression ../../../src/Main.qml
```

The keyboard check opens and resizes real windows, so run it from a graphical
Linux session. Qt's offscreen back end does not match the window, input, or
clipboard behavior that this check covers.
