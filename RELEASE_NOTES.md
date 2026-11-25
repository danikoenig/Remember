Always-on audio recorder for macOS (Apple Silicon), VST3, Audio Unit and CLAP. By Dani Koenig.

### v1.4.2
- **Align grid:** new menu item "Align grid to selection". Plugins in front of Remember (e.g. a drum machine with latency) delay the audio, but the host does not tell Remember. Select 2 or more bars with a steady kick in the history and align: Remember measures how far the kicks lie behind the beat lines and moves the grid by exactly that. Bar lines, snap, BARS buttons, SAVE and DRAG WAV then start on the kick. The offset is stored per plugin instance; "Reset grid offset" sets it back to 0
- Measurement is sample accurate (tests: 70 ms offset found within 1 ms, noise is rejected, nothing left after aligning)

### v1.3.0
- **One grid:** history row, toolbar and playback row share 10 equal columns. SAVE and DRAG WAV have the same size and position in both places
- Toolbar (10 buttons, larger): IMPORT, TO EDITOR, TRIM, FADE IN, FADE OUT, NORMALIZE, REVERSE, FOLD TAIL, UNDO, REDO
- CLEAR moved next to PLAY and LOOP below the editor
- One row below the editor: PLAY, LOOP, CLEAR, NAME box (smaller), KEY, + LOOP (writes Loop into the file name), SAVE, DRAG WAV. The extra row is gone, the window is lower
- Hint line shows the history or the editor controls depending on where the mouse is (right click = copy, cut, paste works in the editor only)
- SELECT BARS centred, disabled buttons keep a clearer outline

### v1.2.0
- **Settings menu extended:** NORMALIZE target (peak, -14 / -16 / -18 / -23 LUFS, -18 / -20 dB RMS; default -18 LUFS), TRIM threshold (-80 to -30 dB), digits of the take number (T01 / T001 / T0001), tooltips on or off
- NORMALIZE now applies the chosen target directly with one click
- TRIM adds a 1 ms fade at both ends, so a high threshold cannot click

### v1.1.1
- **Settings menu** (three bars, top right): export folder (choose / reset), WAV bit depth (16 / 24 / 32 float), fade length, fade shape (smooth, linear, equal power, exponential)
- FADE IN / FADE OUT now simply use these settings, no pop-up on the buttons; FOLDER button removed from the main window

### v1.1.0
- **New layout:** one toolbar in working order, all buttons the same size: IMPORT, TO EDITOR, TRIM, FADE IN, FADE OUT, NORMALIZE, REVERSE, FOLD TAIL, UNDO, REDO, CLEAR
- **Fades:** FADE IN and FADE OUT are separate buttons; right click on either sets the fade length
- **CLEAR** (was DELETE) empties the editor
- **File name:** TYPE, FORM and SCALE removed. KEY is one popup with all notes, major and minor. LOOP IN NAME writes "Loop" into the name
- **Export:** SAVE and DRAG WAV sit in the same columns for the history and the editor; FOLDER chooses the export folder; the big export button is gone

### v1.0.7
- **Logo:** own Remember logo (ring buffer with a trail behind the recording head)

### v1.0.6
- **Design:** header like Drift Station: logo on the left, title with "by Dani Koenig · version" below

### v1.0.5
- **Design:** base colours now match The One and the other plugins (near-black blue-grey, orange frame)

### v1.0.4
- **TO FRONT removed:** one button less, FOLD TAIL stays

### v1.0.3
- **FOLD TAIL:** like Polarity's wrap: the whole selected tail is crossfaded over the start of the clip (equal power) and the clip ends exactly where the selection began

### v1.0.2
- **Names:** WRAP is now FOLD TAIL, ROTATE is now TO FRONT
- **DELETE:** empties the whole editor (Undo brings it back); deleting a selection is in the right-click menu and on the Delete key

### v1.0.1
- **Trim:** after TRIM the first bar line sits on the new start of the clip

### v1.0.0
- **First stable release:** always-on recorder with history, host markers and bar selection, editor (copy, cut, paste, delete, crop, trim, reverse, fades, normalize, undo / redo, preview), wrap, rotate, WAV import, SAVE and DRAG WAV with file-name metadata
- Dark grey design with blue-outlined buttons and the orange frame
- Fewer buttons: Copy, Cut, Paste, Delete and Crop are in the right-click menu of the editor (and Ctrl/Cmd+C, X, V, Delete); fades are one FADE menu; the folder button reads EXPORT TO

### v0.3.1
- **Design:** dark grey look, buttons with a thin blue outline, orange frame kept
- **Rotate:** with a selection (e.g. the tail) its start becomes the new start, the tail moves to the front. Without a selection the green marker is used
- **Wrap:** selected tail is folded onto the start (see v0.3.0)

### v0.3.0
- **Wrap:** select the tail (the reverb after the loop) in the editor and press WRAP; it is folded back onto the start (fade length fades out the tail end)
- **Rotate:** click to set the green marker, ROTATE makes it the new start; length stays the same, a short crossfade removes clicks at the old seam
- **Import:** IMPORT button or drop a WAV / AIFF / FLAC / MP3 onto the plugin; resampled to the host rate, grid from the host tempo if known

### v0.2.0
- **Editor:** TO EDITOR copies the history selection into an editor with its own waveform, tempo grid and snap
- **Functions:** Copy, Cut, Paste, Delete, Crop, Trim (silence), Reverse, Fade in / out (selection or fade length), Normalize (peak -0.1 / -1 dB, LUFS -14 / -16 / -18 / -23 with -0.1 dBFS ceiling), Undo / Redo
- **Preview:** PLAY / LOOP through the plugin output, starts at the green marker (Space)
- **Export:** SAVE and DRAG WAV of the edited clip, same file names as the history

### v0.1.0
- **First version:** continuous history (1 to 20 minutes), waveform with bar and beat markers from the host, play-start marker, start point, selection with snap, SELECT BARS 1 / 2 / 4 / 8 / 16, zoom, HOLD / LIVE
- **Export:** SAVE as 24-bit WAV into a folder of your choice, DRAG WAV into the DAW, file names with project, type, form, BPM, key and take

