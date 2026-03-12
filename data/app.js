function updateLedUI(on) {
  document.getElementById('led').className = 'led-indicator ' + (on ? 'led-on' : 'led-off');
  document.getElementById('ledstate').textContent = on ? 'Allumee' : 'Eteinte';
}

function setLed(on) {
  fetch(on ? '/on' : '/off')
    .then(r => r.json())
    .then(d => updateLedUI(d.led))
    .catch(() => {});
}

var SOUND_ALERT_DB = 75;  // seuil d'alerte en dB (modifiable)

function refreshSensors() {
  fetch('/sensors')
    .then(r => r.json())
    .then(d => {
      document.getElementById('temp').textContent  = d.temperature != null ? d.temperature.toFixed(1) : '--';
      document.getElementById('hum').textContent   = d.humidity    != null ? d.humidity.toFixed(1)    : '--';
      document.getElementById('pres').textContent  = d.pressure    != null ? d.pressure.toFixed(1)    : '--';
      document.getElementById('lux').textContent   = d.lux         != null ? d.lux.toFixed(0)         : '--';

      var sndEl   = document.getElementById('snd');
      var cardEl  = document.getElementById('sound-card');
      if (d.sound_db != null) {
        sndEl.textContent = d.sound_db.toFixed(0);
        if (d.sound_db >= SOUND_ALERT_DB) {
          cardEl.style.border = '2px solid #e53935';
          cardEl.title = 'Niveau sonore eleve !';
        } else {
          cardEl.style.border = '';
          cardEl.title = '';
        }
      } else {
        sndEl.textContent = '--';
        cardEl.style.border = '';
      }

      document.getElementById('upd').textContent = 'Mis a jour : ' + new Date().toLocaleTimeString();
    })
    .catch(() => {});
}

// Chargement initial
fetch('/state')
  .then(r => r.json())
  .then(d => updateLedUI(d.led))
  .catch(() => {});

function refreshTime() {
  fetch('/time')
    .then(r => r.json())
    .then(d => {
      if (d.datetime) {
        // d.datetime = "YYYY-MM-DD HH:MM:SS"
        var parts = d.datetime.split(' ');
        document.getElementById('rtctime').textContent = parts[0] + '  ' + parts[1];
      } else {
        document.getElementById('rtctime').textContent = 'Heure indisponible';
      }
    })
    .catch(() => {});
}

refreshSensors();
refreshTime();
setInterval(refreshSensors, 5000);
setInterval(refreshTime, 1000);
