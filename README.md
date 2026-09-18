# FoldTeX

FoldTeX is a Qt Quick app for writing LaTeX notes on one continuous page.
Text remains directly editable, while formulas render in place. Moving the
cursor into a formula reveals only that expression's source; the surrounding
text and other formulas stay rendered. Text selection, copying, deletion, and
undo work across paragraph boundaries.

FoldTeX currently targets Linux. It is built and tested on Omarchy, which is
based on Arch Linux. There is no package yet. Ubuntu and Debian may work, but
they have not been tested.

## Features

- Save and open `.foldtex` notes; export full notes, figures, structure, and
  course data as `.tex` or `.pdf`.
- Switch between live preview, an editable source view, and a rendered view.
- Mix prose with `$…$` or `\(…\)` inline math; use `\[…\]`, `align`,
  `gather`, `cases`, or matrix environments for larger expressions. Existing
  bare-formula notes are migrated to explicit LaTeX delimiters when opened.
- Display formulas (`$$…$$`, `\[…\]`, `align`, and `equation`) are centered
  within the writing area. Up/Down enters rendered formulas from either side;
  Shift+Up/Down extends the selection through their source.
- Write `\section{…}` and `\subsection{…}` for headings.
- Use `\textbf{…}`, `\emph{…}`, `\textit{…}`, `\texttt{…}` and related text
  commands directly in prose. They show their source when edited and retain
  their formatting in exported LaTeX and PDF.
- Put `\usepackage`, `\newcommand` and other preamble declarations at the top
  of the note. Preview and export use those declarations; an optional
  `\documentclass` is honored too. Use math delimiters around math macros.
- Get command suggestions beside the cursor after typing a backslash and at
  least two letters. Tab accepts a suggestion and navigates snippet fields.
  Tab after the final field moves past the complete snippet, including its
  closing braces and math delimiters. Expansions insert text and select the first
  field in one edit, preserving the viewport while writing in a scrolled note.
- Opening braces and brackets insert a matching closer; typing that closer
  moves over it instead of duplicating it.
- Type `$` for `$|$`, then another `$` immediately for `$$|$$` (`|` is the
  cursor). Typing `\(` or `\[` adds `\)` or `\]` and leaves the cursor inside.
  Type an existing closer to move past it. Backspace removes an empty pair.
  `$`, braces and parentheses can also surround selected text without losing
  the selection. Escaped dollars and dollars in comments are left literal.
- Add `$` immediately before/after an existing inline opening, or after its
  closing `$`, to upgrade both ends to `$$…$$`. Existing double delimiters are
  traversed without adding a third dollar. Inside math or at an unfinished
  closing delimiter, typing `$` inserts only one character. Backspace/Delete
  on either double delimiter reduces both ends to `$…$`; undo restores the pair.
- Finishing `\begin{align}` (or another named environment) inserts its matching
  `\end{align}` and puts the cursor on the empty line between them. An existing
  matching ending is reused. Renaming an environment does not rename its ending.
- Unclosed math delimiters, braces, and environments stay editable without
  being submitted for rendering. Display math separates surrounding prose
  visually while preserving the original source.
- Local Swedish and English spellchecking: misspelled words get a red underline.
  Click or right-click a marked word for suggestions, or ignore it in this note.
  Choose the language under **⋯ → Stavningskontroll**. Swedish is the default
  for new notes and older notes without a saved language. Language, enabled state,
  and ignored words are saved per note; corrections can be undone. Formulas,
  LaTeX commands, preamble declarations, comments and URLs are excluded.
  Dictionaries are bundled; note text is never sent to an online service.
- Find and replace text in both prose and the source behind rendered math.
- Search common LaTeX by English or Swedish words. `Ctrl+K` includes set theory
  and Analysis 1 notation for number sets, intervals, logic, sequences, limits,
  continuity, derivatives, integrals, sup/inf, and Taylor polynomials. Results
  show the Swedish name first and retain the English name beside it.
- Type a short name and press Tab. Along with the core math snippets, FoldTeX
  supports `set`, `union`, `inter`, `abs`, `norm`, `seq`, `series`, `deriv`,
  `eval`, `cint`, `oint`, `epsdel`, `taylor`, and `nn`/`zz`/`qq`/`rr`/`cc`.
  Course shortcuts include `truth`, `induct`, `gcd`, `graph`, `adjacency`,
  `colvec`, `linsys`, `rowswap`, `rowadd`, `charpoly`, `diag`, `orthcomp`,
  `changevar`, `polar`, `flux`, `ode1`, and `ode2`.
- Open the custom snippet manager with `Ctrl+Alt+S`. New snippets work in all
  courses by default, can have several aliases and Tab stops, and can instead
  be limited to one or more named courses. Custom snippets can override a
  built-in trigger, be disabled without deletion, and be imported or exported.
- Add headings, subheadings, bullet and numbered rows, remarks, exercises,
  solutions, and multi-row definitions, theorems, proofs, or examples.
- Start writing immediately with `Ctrl+N`; the current course carries over.
  Set the lecture or problem-solving details later with `Ctrl+Alt+L` or the
  document tools menu.
- Store course and lecture details and search all `.foldtex` notes in the saved
  note's folder.
- Open the note library with `Ctrl+O`. It caches unchanged notes, supports
  multi-word content search, filters by course and note type, shows matching
  rows, and keeps pins, recent notes, and managed note folders.
- LaTeX delimiters determine what renders as math; there is no per-row Math/Text
  switch. Old forced modes are removed on load, so mixed prose and formulas work
  without changing a hidden setting.
- Browse saved notes inside FoldTeX with `Ctrl+O`; search their metadata and
  content, sort them by update time, lecture date, title, or course, and add
  older note folders without leaving the app.
- Paste a clipboard image into a saved note, then edit its caption or replace
  it from the row menu. FoldTeX stores it next to the note.
- Autosave saved notes after one second of idle time. Each note also gets its
  own recovery copy and up to 30 timed snapshots, all shown in the Recovery
  screen. Checked saves do not overwrite a file changed by another app.
- Save As copies figures and lecture slides and stores their links as paths
  relative to the note, so the full note folder can move to another computer.
- Paste multi-line text while preserving complete LaTeX environments.
  Backspace and Delete join paragraphs at their boundaries.
- Set the writing font, size, and side margin. The margin shrinks on narrow
  windows, and both source and rendered math track the chosen size.
- Draw figures with a pen, eraser, lines, arrows, boxes, and text, then insert
  them as image rows. Unsaved notes keep figures temporarily and move them into
  the note's asset folder on first save. `Ctrl+Z` removes the last drawing
  action while the figure editor is open.
- Copy a lecture PDF beside a saved note and view it next to the note. Link a
  row to a slide or capture the visible slide area as an image row.

Explicit `\\` in prose folds into a line break while its source stays intact
for copying. LaTeX export keeps ordinary line breaks and translates a trailing
break into paragraph spacing; display math supplies its own line boundaries.
A following source newline does not add a second
break, and a break at the end of a row reuses the existing paragraph boundary.
Move the cursor onto the command or use source view to edit its backslashes.

## Shortcuts

- `Enter`: insert a source newline at the cursor; keep math environments together
- `Ctrl+Enter`: insert an empty row below the row being edited
- `Shift+Enter`: start a new source and rendered line within the same block,
  keeping the cursor there. Works for text and math; FoldTeX adds a left-aligned
  `aligned` wrapper for math when needed and splits `\text{…}` at the cursor.
- `Up` or `Down`: move through the document while retaining the cursor column
- Drag, `Shift+click`, or `Shift` + arrows: select continuous source text
- `Ctrl+A` or `Ctrl+Shift+A`: select the document; copy, cut, Backspace, and
  Delete act on the selected text, including across paragraphs
- `Ctrl+N`: start an empty note immediately; the previous note gets recovery data
- `Ctrl+S`, `Ctrl+Shift+S`, `Ctrl+O`: save, save as, and open the notes library
- `Ctrl+Q`: save recovery data and quit
- `Ctrl+Z`, `Ctrl+Shift+Z`: undo and redo
- `Tab`, `Shift+Tab`: move forward and back through snippet fields
- `Ctrl+F`: find and replace in the note
- `Ctrl+K`: find and insert LaTeX by name
- `Ctrl+G`: open the searchable LaTeX guide
- `Ctrl+.`: set the current row type; choose with Up/Down and Enter
- `Alt+Up`, `Alt+Down`: move the current row
- `Ctrl+D`: copy the current row
- `Ctrl+M`: add a timed catch-up mark
- `Ctrl+Alt+L`: set course and lecture details
- `Ctrl+Alt+F`: search notes in the current note's folder
- `Ctrl+Alt+S`: manage custom Tab snippets
- `Ctrl+Shift+V`: paste a clipboard image into a saved note
- `Ctrl+Alt+I`: draw a figure
- `Ctrl+Alt+P`: add, show, or hide lecture slides
- `Ctrl+Shift+R`: cycle through automatic, source, and rendered views
- `Ctrl+Shift+E`: export as TeX or PDF
- `Ctrl+'`, `Ctrl+,`, or `Ctrl+Shift+F`: set the font, size, and side margin
- `F1`: show keyboard help

The visible `⋯` menu at the top right opens note details, settings, help,
LaTeX tools, figures, slides, view options, and export. The guide also supports Up, Down, Page Up, Page Down,
and Escape.

Multi-line blocks stay together when saved, reopened, recovered, or exported.
New saves use `foldtex-3`; older notes are migrated when opened. Use this
version of FoldTeX to open new saves, as older versions split source newlines
into separate blocks.

## Figures and lecture slides

The figure tool keeps new drawings temporarily until the first save, then puts
them beside the note in its `.assets` folder. It has a pen, eraser, straight
line, arrow, box, text, undo, and clear. After insertion, FoldTeX focuses a text
row below the figure so writing can continue.

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
sudo pacman -S --needed base-devel qt6-base qt6-declarative hunspell texlive-latexextra dvisvgm
```

This set is tested on Omarchy. `qt6-base` supplies `qmake6`, and
`texlive-latexextra` pulls in the other TeX Live sets used by FoldTeX.

For Ubuntu 24.04 LTS, these package names are present in the Ubuntu repos:

```sh
sudo apt update
sudo apt install build-essential qt6-base-dev qt6-declarative-dev libhunspell-dev texlive-latex-extra dvisvgm
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

To install or update the app for the current user:

```sh
qmake6 foldtex.pro PREFIX="$HOME/.local"
make -j"$(nproc)"
make install
update-mime-database "$HOME/.local/share/mime"
```

The install adds the executable, desktop entry, and `.foldtex` file type under
`~/.local`. You can then open a note from the app menu or pass its file name to
`foldtex`.

## Tests

From the repository root, build and run both checks with:

```sh
mkdir -p build/tests/backend build/tests/document-editor

cd build/tests/backend
qmake6 ../../../tests/tests.pro
make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= \
  ./backend_regression ../../../tests/fixtures/multiline.foldtex

cd ../document-editor
qmake6 ../../../tests/document_editor_tests.pro
make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME= \
  ./document_editor_regression ../../../src/Main.qml
```

The continuous-editor check uses real Qt key and mouse events with an isolated
application data directory. It also compiles math and exports a PDF. The old
`keyboard_regression.cpp` describes the retired per-row widgets and is retained
as a reference for the remaining dialog-specific test migration.

The long-lecture check builds from `tests/lecture_stress.pro`. It loads 1,500
mixed text, math, block, and list rows and checks common edit and search work
against fixed time limits. See `tests/LECTURE_STRESS.md` for the command.

## Continuous editor checks

Build `tests/document_editor_tests.pro` in `build/tests/document-editor`, then
run `document_editor_regression ../../../src/Main.qml`. The test exercises
actual key events, cross-paragraph selection, cursor placement, Unicode
deletion, undo/redo, paste, independent inline previews, snippets, source mode,
PDF export, and save/reopen. For an isolated headless run use
`QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=`.
The block-break test remains a separate integration check for Shift+Enter.

The continuous editor preserves the exact source in `.foldtex` files.
Rendered math carries source positions for its atoms, including fraction
arguments and scripts. Clicking a symbol opens the corresponding source;
user macro expansions map to their invocation. If an unusual TeX construct
cannot accept source markers, it still renders normally and uses an approximate
click position. Native text formatting keeps character-level positions.

Preamble packages must be installed in the local TeX distribution and compatible
with its LaTeX/DVI renderer. The page remains a live fragment preview; full
pagination and document-wide cross-references belong to the PDF export.
See [the implementation audit](IMPLEMENTATION_AUDIT.md) for the original
interaction proposals, their verification, and the remaining gaps.

Spellchecking uses the system Hunspell library and bundled Swedish/English
dictionaries from LibreOffice. Dictionary sources, pinned revision and license
notices are in [third_party/dictionaries/SOURCES.md](third_party/dictionaries/SOURCES.md).
The spelling regression suite is `tests/spelling_tests.pro` (run its executable
with the absolute path to `src/Main.qml`).
