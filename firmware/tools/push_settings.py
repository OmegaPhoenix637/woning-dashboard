"""Zet alle instellingen uit firmware/secrets.json via de USB-kabel op de ESP32.

Gebruik (in de map firmware):
    python tools/push_settings.py            (standaard COM11)
    python tools/push_settings.py COM5

- Lege waarden ("") worden overgeslagen: wat al op de ESP staat blijft dan staan.
- secrets.json staat NIET op GitHub (zie .gitignore).
"""
import json
import pathlib
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM11"
path = pathlib.Path(__file__).resolve().parent.parent / "secrets.json"
if not path.exists():
    sys.exit(f"Niet gevonden: {path}\nKopieer secrets.example.json naar secrets.json en vul hem in.")

data = json.loads(path.read_text(encoding="utf-8"))
items = {}
for k, v in data.items():
    if k.startswith("_") or v is None or v == "":
        continue
    if isinstance(v, bool):
        v = "1" if v else "0"
    items[k] = str(v).strip()

s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.2
s.dtr = False  # niet resetten bij openen
s.rts = False
s.open()


def ask(cmd: str, wait: float = 1.2, until: str = "") -> str:
    """Stuur een opdracht en lees het antwoord. Met 'until' wordt gewacht tot die tekst verschijnt."""
    s.reset_input_buffer()
    s.write((cmd + "\n").encode("utf-8"))
    t, out = time.time(), b""
    while time.time() - t < wait:
        out += s.read(4096)
        if until and until.encode() in out:
            break
    return out.decode("utf-8", "replace")


print(f"Verbinden met de ESP op {port}...")
for _ in range(60):
    if "wifi_ssid" in ask("show", 1.5):
        break
else:
    sys.exit("De ESP reageert niet. Zit de USB-kabel erin en is het de juiste COM-poort?")

ok, bad = [], []
for k, v in items.items():
    if "\n" in v:
        bad.append(k)
        continue
    r = ask(f"set {k} {v}", 10, until=f"{k} ")
    (ok if f"{k} opgeslagen" in r else bad).append(k)
    print(f"  {k:<14} {'opgeslagen' if f'{k} opgeslagen' in r else 'MISLUKT: ' + r.strip()[:80]}")

print(f"\n{len(ok)} instelling(en) opgeslagen" + (f", {len(bad)} mislukt: {', '.join(bad)}" if bad else "."))
print("ESP herstarten...")
s.write(b"restart\n")
time.sleep(0.5)

# Meekijken of de wifi lukt
t, log = time.time(), ""
while time.time() - t < 75:
    log += s.read(4096).decode("utf-8", "replace")
    if "Wifi OK" in log or "Instelmodus" in log:
        time.sleep(1)
        log += s.read(4096).decode("utf-8", "replace")
        break
s.close()
for line in log.splitlines():
    if any(w in line for w in ("Verbinden", "mislukt", "Wifi OK", "Instelmodus", "zichtbaar:")):
        print("  " + line.strip())
if "Wifi OK" in log:
    ip = log.split("Wifi OK, IP ")[-1].split()[0]
    print(f"\nKlaar! Open http://energie.local of http://{ip}")
