/*
*
*/
const gateway = `ws://localhost:8080`;
//const gateway = `ws://${window.location.hostname}/ws`;
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

fileInput.addEventListener("change", async e => {
  const file = e.target.files[0];
  const arrayBuffer = await file.arrayBuffer();
  midiData = new Midi(arrayBuffer);
  prepareNotes();
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