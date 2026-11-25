# Remember

Always-on Audio-Recorder als Plugin (macOS Apple Silicon, VST3, Audio Unit, CLAP). Von Dani Koenig.

Remember laeuft auf einer Spur oder einem Bus mit, nimmt alles auf und behaelt die letzten 1 bis 20 Minuten im Speicher. Was gut klang, waehlst du hinterher aus und ziehst es als WAV in die DAW. Das Audio wird nicht veraendert.

**Workflow:** Record - Select - Export

- Ringpuffer mit 1, 2, 5, 10 oder 20 Minuten History (Speicher wird nur fuer die gewaehlte Laenge belegt)
- Host-Marker: Play-Start, Takte und Beats aus dem Transport der DAW, Raster folgt dem Host-Tempo
- Auswahl per Klick (gruener Startpunkt), Ziehen oder Doppelklick (Takt); Buttons fuer 1 / 2 / 4 / 8 / 16 Takte; Snap auf 1/16, 1/8, Beat oder Takt (Ctrl / Cmd = frei)
- Zoom mit Mausrad, Shift + Rad scrollt, F zeigt alles, Hold friert die Ansicht ein
- SAVE schreibt die Auswahl als 24-Bit-WAV in den Export-Ordner, DRAG WAV zieht sie in die DAW
- Dateinamen mit Projekt, Typ, Form, BPM, Key und Take (`MyTune_Synth_Loop_124bpm_Amin_T03.wav`)

Editor (v0.2): TO EDITOR, Copy / Cut / Paste, Delete, Crop, Trim, Reverse, Fades, Normalize (Peak oder LUFS), Undo / Redo, Vorhoeren mit Loop, Export des bearbeiteten Clips.
Wrap und Rotate fuer nahtlose Loops, WAV-Import per Button oder Drop (v0.3).

Installation: siehe INSTALL.txt. Build: GitHub Actions (siehe .github/workflows/build.yml).

## Entwicklung

- Test der Engine ohne JUCE: `g++ -std=c++17 -O1 -Wall tests/EngineTest.cpp Source/Engine.cpp -o enginetest && ./enginetest`
- Oberflaeche als PNG rendern (mit Testaudio und simuliertem Host, Linux oder macOS): `cmake -B build -DREMEMBER_SNAPSHOT_TOOL=ON && cmake --build build --target RememberSnapshot`, dann `RememberSnapshot out.png 4`

Der Audio-Thread schreibt nur in vorab angelegten Speicher (keine Allokation, keine Locks, keine Dateien). Export, Wellenform und Dateien laufen ausserhalb des Audio-Threads.
