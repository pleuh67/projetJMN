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

function refreshSensors() {
  fetch('/sensors')
    .then(r => r.json())
    .then(d => {
      document.getElementById('temp').textContent  = d.temperature != null ? d.temperature.toFixed(1) : '--';
      document.getElementById('hum').textContent   = d.humidity    != null ? d.humidity.toFixed(1)    : '--';
      document.getElementById('pres').textContent  = d.pressure    != null ? d.pressure.toFixed(1)    : '--';
      document.getElementById('lux').textContent   = d.lux         != null ? d.lux.toFixed(0)         : '--';
      document.getElementById('upd').textContent   = 'Mis a jour : ' + new Date().toLocaleTimeString();
    })
    .catch(() => {});
}

// Chargement initial
fetch('/state')
  .then(r => r.json())
  .then(d => updateLedUI(d.led))
  .catch(() => {});

refreshSensors();
setInterval(refreshSensors, 5000);
