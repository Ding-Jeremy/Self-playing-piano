/*
*
*/
//const gateway = `ws://localhost:8080`;
const gateway = `ws://${window.location.hostname}/ws`;
let websocket;

// Sends note to the ESP-32 that are that time away

window.addEventListener("load", onLoad);

// Web socket 
function onLoad() {
  initWebSocket();
}

// Piano graphical interface
let audio = true;
const viewer = document.getElementById("viewer");
const piano = document.getElementById("piano");
const playPauseBtn = document.getElementById("playPause");
const fileInput = document.getElementById("fileInput");
const restartBtn = document.getElementById("restart");
const LOOKAHEAD_MS = 0.5;

let nextNoteIndex = 0;   // cursor into notes[]

// HTML FORMS
restartBtn.onclick = () => {
  // Always reset time
  pauseTime = 0;
  startTime = performance.now();

  // Reset visuals
  display_notes(time_delta);
  document.getElementById("timeDisplay").textContent = formatTime(0);

  if (!paused) {
    // Was playing → keep playing
    requestAnimationFrame(animate);
  } else {
    // Was paused → stay paused
    playPauseBtn.textContent = "Play";
  }
};

// On file input
fileInput.addEventListener("change", async e => {
  const file = e.target.files[0];
  const arrayBuffer = await file.arrayBuffer();
  midiData = new Midi(arrayBuffer);
  prepareNotes();
  // Track-level info
  const trackInfo = {
      ticksPerBeat: midiData.header.ppq, // pulses per quarter note
      tempo: midiData.header.tempos[0].bpm || 120, // first tempo in BPM
      trackCount: midiData.tracks.length
  };
  // Send track infos 
  websocket.send(JSON.stringify({ type: "track_info", data: trackInfo }));
  // Show notes
  display_notes(time_delta);
});

playPauseBtn.onclick = () => {
  paused = !paused;
  playPauseBtn.textContent = paused ? "Play" : "Pause";

  if (!paused) {
    
    // Compute time
    startTime = performance.now() - pauseTime;
    const time_info = {
      time: startTime
    };
    // resume
    websocket.send(JSON.stringify({type: "resume",data:time_info}));
    requestAnimationFrame(animate);
  } else {
    // pause
    websocket.send(JSON.stringify({type: "pause"}));
    pauseTime = performance.now() - startTime;
  }
};

// Piano
generate_piano();

// Animate the down coming of the notes
function animate(time) {
  // Kill the sound in case of pause
  if (paused){
    //synth.releaseAll();
    return;
  }
  // Compute elapsed time [s]
  const elapsed = (time - startTime) * 0.001+time_delta;

  // Update elapsed time on screen
  document.getElementById("timeDisplay").textContent= formatTime(elapsed);
  // Html objects
  keys_currently_played = [];

  // Get currently played notes
  const notes_currently_played = new Set();
  for (let i = 0; i < notes.length; i++) {
    const obj = notes[i];
    const midi = obj.note.midi;
    const noteStart = obj.note.time;
    const noteEnd = obj.note.time + obj.note.duration;

    // Check for notes that need to be played currently
    if (elapsed >= noteStart && elapsed < noteEnd) {
        // Save tone and current note id
        notes_currently_played.add(i);
        keys_currently_played.push(get_key(midi));
    }
  }
  // Graphical interface
  display_notes(elapsed);

  // Highlight currently played keys
  highlight_notes(keys_currently_played);
  send_notes(elapsed);
  requestAnimationFrame(animate);
}

function send_notes(elapsed) {
  const windowEnd = elapsed + LOOKAHEAD_MS;

  while (
    nextNoteIndex < notes.length &&
    notes[nextNoteIndex].note.time <= windowEnd
  ) {
    const note = notes[nextNoteIndex].note;

    send_note_to_esp(note);

    nextNoteIndex++;
  }
}

function send_note_to_esp(note) {
  const msg = {
    time: Math.round(note.time*1000),        // absolute ms from song start
    midi: note.midi-START_NOTE,
    duration: Math.round(note.duration*1000), //ms
    velocity: Math.round(note.velocity*255),
    on: 1,
  };

  websocket.send(JSON.stringify({ type: "note", data: msg }));
}
