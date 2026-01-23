// MIDI player


// Graphics constants
const NOTE_HEIGHT = 0;
const PIANO_WIDTH = piano.clientWidth*8/10;

const WHITE_NOTE_WIDTH = PIANO_WIDTH/52;
const BLACK_NOTE_WIDTH = WHITE_NOTE_WIDTH*5/10;

const WHITE_NOTE_HEIGHT = WHITE_NOTE_WIDTH*5;
const BLACK_NOTE_HEIGHT = BLACK_NOTE_WIDTH*7;

const PIANO_START_X = (piano.clientWidth-PIANO_WIDTH)/2;
const PIANO_START_Y = viewer.clientHeight;
const FALL_SPEED = 0.1  ;
const START_NOTE = 21; // C3
const END_NOTE = 108;   // C6
const NOTE_START_HEIGHT = 100;
// Piano constants (physical)
const PHY_MIN_OFF_ON_DELAY = 100; // ms
const PHY_MAX_CONCURENT_NOTES = 10;

// Variables
let midiData = null;
let notes = [];
let startTime = null;
let paused = true;
let pauseTime = 0;
let white_key_counter = 0;
let keys = null;
let time_delta = 1; // Seconds before the song starts
const events = [];  // Create events (those are sent to the ESP, not the notes.)

// Prepare all notes from midi file.
function prepareNotes() {
  viewer.innerHTML = "";
  notes = [];

  // Read tracks and add notes
  midiData.tracks.forEach(track => {
    track.notes.forEach(note => {
      const el = document.createElement("div");
      el.className = "note";
      
      el.style.width = note_width(note.midi);
      el.style.left = noteX(note.midi);
      el.style.height = `${note.duration}px`;
      el.style.willChange = "transform";
      el.style.visibility="hidden";
      viewer.appendChild(el);
      notes.push({ note, el });
    });
  });
  // Sort notes in time
  notes.sort((a, b) => a.note.time - b.note.time);

  configure_events();
}

/*
* Analyse notes, add ON OFF argument, check for validity. Moves all notes forward from
* time delta.
* adapt notes format.
* adjust for physical limitations.
*/
function configure_events(){
  // Move all notes forward from time delta
  // Adjust time value (ms)
  for (let note of notes){
    note.note.time += time_delta;
    note.note.time = Math.round(note.note.time*1000); // ms
    note.note.duration = Math.round(note.note.duration*1000); // ms
  }

    // Create events
  for (let note of notes) {
    // NOTE ON
    events.push({
      midi: note.note.midi,
      time: note.note.time,
      on: 1,
      noteRef: note  
    });

    // NOTE OFF
    events.push({
      midi: note.note.midi,
      time: note.note.time + note.note.duration,
      on: 0,
      noteRef: note  
    });
  }

  // Now check for notes restarts (add mandatory waiting time. (reduce note duration, and add a note off))
  // Loops through each keys
  const activeNoteByMidi = new Map(); // midi -> note object

  events.sort((a, b) => {
  if (a.time !== b.time) return a.time - b.time;
    return a.on - b.on; // off (0) before on (1)
  });

  for (let e of events) {
    const midi = e.midi;

    if (e.on === 1) {
      // NOTE ON
      if (activeNoteByMidi.has(midi)) {
        const prev = activeNoteByMidi.get(midi);

        // shorten previous note, do NOT move timing
        const forcedOff = e.time - PHY_MIN_OFF_ON_DELAY;
        prev.note.duration = Math.max(
          0,
          forcedOff - prev.note.time
        );
      }

      activeNoteByMidi.set(midi, e.noteRef);

    } else {
      // NOTE OFF
      if (activeNoteByMidi.get(midi) === e.noteRef) {
        e.noteRef.note.duration = Math.max(
          0,
          e.time - e.noteRef.note.time
        );
        activeNoteByMidi.delete(midi);
      }
    }
  }

  // Update notes height
  for (let note of notes) {
    note.el.style.height =
      `${note.note.duration * FALL_SPEED}px`;
  }
}


/*
* Returns if a given midi note is black or white
*/
function isBlack(midi_note){
  const black_keys = [1,3,6,8,10];
  const note_id = midi_note-START_NOTE;
  // Start of the piano
  if (note_id < 3){
    if (note_id == 1){
      return true;
    }else{
      return false;
    }
  }else{
    const note = (note_id-3) % 12;
    const isBlack = black_keys.includes(note);
    if (isBlack){
      return true;
    }else{
      return false;
    } 
  }
}

function generate_piano(){
    // Generate piano display (create notes)
    for (let n = START_NOTE; n <= END_NOTE; n++) {
    // Compute key position
    const note_id = n - START_NOTE;
    const position_x = note_id*WHITE_NOTE_WIDTH;
    const key = document.createElement("div");
    // Start of the piano
    if (isBlack(n)){
        key.className = "key black";
    }else{
        key.className = "key white";
    }
    // Set position and dimensions
    if (key.className == "key white"){
        key.style.left = `${white_key_counter*WHITE_NOTE_WIDTH+PIANO_START_X}px`;
        key.style.width = `${WHITE_NOTE_WIDTH}px`;
        key.style.height = `${WHITE_NOTE_HEIGHT}px`;
        white_key_counter++;
    }else{
        const x_pos = (white_key_counter)*WHITE_NOTE_WIDTH-BLACK_NOTE_WIDTH/2+PIANO_START_X;

        key.style.left = `${x_pos}px`;
        key.style.width = `${BLACK_NOTE_WIDTH}px`;
        key.style.height = `${BLACK_NOTE_HEIGHT}px`;
    }
    piano.appendChild(key);
    }
}

// Save piano keys
keys = piano.getElementsByClassName("key");

// Return key object from midi note
function get_key(midi){
  // Return the key based on the midi number
  return keys[midi-START_NOTE];
}

// Get note x coordinates from midi note
function noteX(midi) {
  // Return the note x position
  const note_id = midi-START_NOTE;
  return keys[note_id].style.left;
}

function note_width(midi){
  // Return corresponding width for a given note
  if (isBlack(midi)){
    return ""+BLACK_NOTE_WIDTH+"px";
  }
  return ""+WHITE_NOTE_WIDTH+"px";
}

// Track which MIDI notes are currently sounding
const activeNotes = new Set();
let viewerHeight = 0;

// Audio player
/*const synth = new Tone.PolySynth(Tone.Synth, {
  maxPolyphony: 30,

}).toDestination();

// Make sure audio is unlocked once
let audioStarted = false;

async function ensureAudioStarted() {
  if (!audioStarted) {
    await Tone.start();
    audioStarted = true;
  }
}*/

function updateViewerSize() {
  viewerHeight = viewer.clientHeight;
}

window.addEventListener("resize", updateViewerSize);
updateViewerSize();

function display_notes(elapsed){
  // Display notes on the screen
  for (let i = 0; i < notes.length; i++) {
    const obj = notes[i];

    // Y coordinates of the bottom of the key
    const y_bot =
      viewerHeight -
      (obj.note.time - elapsed) * FALL_SPEED -
      NOTE_HEIGHT;
    const current_height = obj.note.duration*FALL_SPEED;
    // Cut off-screen notes
    if (y_bot < 0 || y_bot-current_height > viewerHeight || obj.note.on == "0"){
      obj.el.style.visibility="hidden";
      continue;
    }
    // Notes currently on screen
    obj.el.style.visibility="visible";
    obj.el.style.transform = `translate3d(0, ${y_bot-current_height}px, 0)`;
  }
}

function highlight_notes(keys_currently_played){
  for (let key of keys){
    if (keys_currently_played.includes(key)){
      key.style.background="#00FF00";
    }else{
      if (key.className == "key white"){
        key.style.background="#FFFFFF";
      }else{
        key.style.background="#000000";
      }
    }
  }
}

// Format a time in [mm:ss:cs]
function formatTime(seconds) {
  const m  = Math.floor(seconds / 60);
  const s  = Math.floor(seconds % 60);
  const cs = Math.floor((seconds * 100) % 100); // centiseconds

  return (
    String(m).padStart(2, "0") + ":" +
    String(s).padStart(2, "0") + ":" +
    String(cs).padStart(2, "0")
  );
}
