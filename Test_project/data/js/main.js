/*
*
*/
//const gateway = `ws://localhost:8080`;
const gateway = `ws://${window.location.hostname}/ws`;
let websocket;

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
const notes_buffer_size = 50;
let notes_buffer_duration = null; // Defines the time needed to complete the buffer

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
  websocket.send(JSON.stringify({ type: "trackInfo", data: trackInfo }));
  // Show notes
  display_notes(time_delta);
});

playPauseBtn.onclick = () => {
  paused = !paused;
  playPauseBtn.textContent = paused ? "Play" : "Pause";

  if (!paused) {
    // resume or start
    startTime = performance.now() - pauseTime;
    requestAnimationFrame(animate);
  } else {
    // pause
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
  // ONLY FOR AUDIO
  // Turn on notes that are in keys_currently_played but not active
  /*for (let id of notes_currently_played) {
    if (!activeNotes.has(id)) {
      const midi = notes[id].note.midi;
      const vel = notes[id].note.velocity;
      const noteName = Tone.Frequency(midi,"midi").toNote();

      synth.triggerAttack(noteName,undefined,vel);
      activeNotes.add(id);
    }
  }

  // Turn off notes that are active but no longer in keys_currently_played
  for (let id of activeNotes) {
    if (!notes_currently_played.has(id)) {
      const midi = notes[id].note.midi;
      const noteName = Tone.Frequency(midi, "midi").toNote();        
      synth.triggerRelease(noteName);
      activeNotes.delete(id);
    }
  }*/
 
  requestAnimationFrame(animate);
}

function send_notes(elapsed){
  // sends notes when needed, (relative to time elapsed and note buffer and tick rate)
  // First call (nothing sent yet)
  if (notes_buffer_duration == null){
    // Compute the duration
    notes_buffer_duration = 0;
    for (let i = 0; i < notes_buffer_size; i++){
      notes_buffer_duration += notes[i].note.midi.duration;
    }
    // Send first notes
    const chunk = notes.slice(0, notes_buffer_size).map(n => ({
      note: n.note.midi,
      duration: n.note.durationTicks,
      time: n.note.ticks,
      velocity: round(n.note.velocity*100)
    }));

    websocket.send(JSON.stringify({ type: "note_buffer", data: chunk }));
  }else{
    // Check if time before last sent approches the buffer duration, if so, send the next one.

  }
}
    