'use strict';
const byId = id => document.getElementById(id);
const media = byId('testAudio');
const random = new Uint32Array(4);
crypto.getRandomValues(random);
const client = Array.from(random, value => value.toString(16).padStart(8, '0')).join('');
byId('wake').muted = false;
byId('wake').volume = 1;
byId('wake').addEventListener('timeupdate', () => {
  if (byId('wake').currentTime > 1) byId('wake').currentTime = 0.1;
});
let armed = false, loading = false, lastCommand = 0, lastContact = Date.now(), playingCommand = 0, awake;
const payload = event => ({client, event, armed, playing: !media.paused, audio_ready: media.readyState >= 2,
  visible: !document.hidden, video_playing: !byId('wake').paused, video_time: byId('wake').currentTime,
  media_time: media.currentTime, media_duration: media.duration, media_muted: media.muted, media_volume: media.volume,
  protocol: 2, media_source: media.currentSrc, playback_method: 'HTMLAudioElement', audio_session: navigator.audioSession?.type || 'unsupported',
  command_id: playingCommand, user_agent: navigator.userAgent, client_time: Date.now() / 1000});
async function report(event) {
  await fetch('/event', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload(event))});
}
function stopAudio() { media.pause(); }
function disconnect(message) {
  armed = false;
  stopAudio();
  byId('wake').pause();
  awake?.release().catch(() => {});
  byId('enable').disabled = false;
  byId('status').textContent = 'Disconnected';
  byId('detail').textContent = message;
  report('disconnected').catch(() => {});
}
byId('stop').onclick = () => disconnect('Tap Enable speaker when you want to reconnect.');
byId('enable').onclick = async () => {
  if (loading) return;
  loading = true;
  byId('enable').disabled = true;
  try {
    if (navigator.audioSession) navigator.audioSession.type = 'playback';
    media.muted = false;
    media.volume = 1;
    // Unlock this same media element in the tap. The file begins with two silent seconds.
    const mediaStarted = media.play().then(() => { media.pause(); media.currentTime = 0; });
    const videoStarted = byId('wake').play().then(() => {
      byId('awake').textContent = 'Silent media keeps the screen awake. Leave this page open.';
    }).catch(() => { byId('awake').textContent = 'Keep-awake video blocked. Keep the screen unlocked.'; });
    byId('status').textContent = 'Connecting…';
    await mediaStarted;
    await videoStarted;
    armed = true;
    lastContact = Date.now();
    byId('status').textContent = 'Ready';
    byId('detail').textContent = 'Leave the phone here. Countdown, playback, and recording are automatic.';
    await report('ready');
    if (navigator.wakeLock) {
      try { awake = await navigator.wakeLock.request('screen'); byId('awake').textContent = 'Screen kept awake. Leave this page open.'; }
      catch (_) { /* The silent video remains the fallback on local HTTP. */ }
    }
  } catch (error) { disconnect(error.message); }
  finally { loading = false; }
};
media.onended = () => {
  byId('status').textContent = 'Ready';
  byId('detail').textContent = 'Playback finished. Waiting for the next test.';
  report('ended').catch(() => {});
};
media.onerror = () => disconnect('Audio playback failed. Reload and enable the speaker again.');
document.addEventListener('visibilitychange', () => {
  if (document.hidden && armed) disconnect('Page left the foreground. Return and tap Enable speaker.');
});
async function poll() {
  try {
    const response = await fetch('/state');
    if (!response.ok) throw new Error('Connection lost');
    const command = await response.json();
    lastContact = Date.now();
    if (command.client === client && command.id !== lastCommand) {
      lastCommand = command.id;
      if (command.action === 'stop') { stopAudio(); await report('stopped'); }
      if (command.action === 'play') {
        if (!armed || document.hidden) { await report('play_rejected'); }
        else {
          stopAudio();
          playingCommand = command.id;
          if (navigator.audioSession) navigator.audioSession.type = 'playback';
          if (command.source && /^phone-[a-z0-9-]+\.wav$/.test(command.source)) {
            media.src = '/' + command.source;
          }
          media.currentTime = 0;
          try {
            await media.play();
            byId('status').textContent = 'Playing test tones';
            byId('detail').textContent = 'Keep the phone still and its volume unchanged.';
            await report('started');
          } catch (error) { disconnect(error.message); }
        }
      }
    }
  } catch (_) {
    if (armed && Date.now() - lastContact > 5000) disconnect('Connection lost. Reconnect when the page is reachable.');
  } finally { setTimeout(poll, 250); }
}
setInterval(() => {
  if (armed) report('heartbeat').catch(() => {});
  byId('progress').max = Number.isFinite(media.duration) ? media.duration : 35;
  byId('progress').value = media.currentTime;
  if (playingCommand) byId('timer').textContent = `${media.currentTime.toFixed(1)} / ${Number.isFinite(media.duration) ? media.duration.toFixed(0) : '?'} s`;
}, 1000);
poll();
