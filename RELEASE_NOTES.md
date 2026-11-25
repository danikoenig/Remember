Always-on audio recorder for macOS (Apple Silicon), VST3, Audio Unit and CLAP. By Dani Koenig.

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

