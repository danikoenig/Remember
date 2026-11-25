Always-on audio recorder for macOS (Apple Silicon), VST3, Audio Unit and CLAP. By Dani Koenig.

### v0.3.0
- **Wrap:** select the loop in the editor; the reverb tail after it is folded back onto the start (fade length fades out the tail end), material before the loop is dropped
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

