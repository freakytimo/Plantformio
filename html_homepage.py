from datetime import datetime
import webbrowser
import tempfile

# Dummy-Daten
NUM_RELAYS = 4
relayState = ["on", "off", "on", "off"]
onTimes = ["08:00", "09:00", "10:00", "11:00"]
offTimes = ["18:00", "19:00", "20:00", "21:00"]
timerEnabled = [True, False, True, False]
fanSpeedPercent = 55

current_time = datetime.now().strftime("%d.%m.%Y %H:%M:%S")

html = """
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="60">
<style>
html {{ font-family: Helvetica; text-align: center; }}
.button {{ padding: 10px 25px; font-size: 20px; border: none; color: white; margin: 10px; cursor: pointer; }}
.on-button {{ background-color: #4CAF50; }}
.off-button {{ background-color: #9d1d14; }}
.blue-button {{ background-color: #003366; }}
.row {{ display: flex; justify-content: center; flex-wrap: wrap; }}
.box {{ width: 20%; min-width: 200px; margin: 10px; border: 1px solid #ccc; padding: 10px; box-sizing: border-box; }}
</style></head><body>

<h2>Plantformio</h2>
<p><b>Uhrzeit:</b> {}</p>
<div class='row'>
""".format(current_time)

for i in range(NUM_RELAYS):
    status = relayState[i].capitalize()
    url = f"/{i+1}/{'on' if relayState[i] == 'off' else 'off'}"
    label = "Einschalten" if relayState[i] == "off" else "Ausschalten"
    btn_class = "button on-button" if relayState[i] == "off" else "button off-button"
    html += f"""
    <div class='box'>
    <p><b>Relais {i+1}</b><br>Status: {status}</p>
    <a href='{url}'><button class='{btn_class}'>{label}</button></a>
    </div>
    """

html += "</div><h3>Zeitschaltuhr</h3><form action='/set' method='GET'><div class='row'>"

for i in range(NUM_RELAYS):
    checked = "checked" if timerEnabled[i] else ""
    html += f"""
    <div class='box'>
    <p><b>Relais {i+1}</b></p>
    <p><label style='margin-left:4px;'>Ein:  <input type='time' name='on{i+1}' value='{onTimes[i]}' required></p>
    <p>Aus: <input type='time' name='off{i+1}' value='{offTimes[i]}' required></p>
    <input type='hidden' name='timer{i+1}' value='0'>
    <p>Timer aktiv: <input type='checkbox' name='timer{i+1}' value='1' {checked}></p>
    </div>
    """

html += """
</div>
<input type='submit' class='button blue-button' value='Zeiten setzen'>
</form>

<h3>Lueftersteuerung</h3>
<form action='/set' method='GET'>
<p>Aktuell: <output id='fanVal'>{}%</output></p>
<input type='range' min='0' max='100' value='{}' name='fan' oninput='fanVal.value = this.value' onchange='this.form.submit();'>
</form>
</body></html>
""".format(fanSpeedPercent, fanSpeedPercent)

# Datei erzeugen und im Browser anzeigen
with tempfile.NamedTemporaryFile("w", delete=False, suffix=".html") as f:
    f.write(html)
    webbrowser.open("file://" + f.name)
