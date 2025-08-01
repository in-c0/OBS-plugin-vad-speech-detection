import sounddevice as sd
import numpy as np
import queue
import json
import socket
from vosk import Model, KaldiRecognizer

# Config
model_path = "models/vosk-model-small-en-us-0.15"
udp_ip = "127.0.0.1"
udp_port = 7000

# Setup Vosk
model = Model(model_path)
rec = KaldiRecognizer(model, 16000)
rec.SetWords(True)

# Setup UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Audio stream
q = queue.Queue()
def callback(indata, frames, time, status):
    if status: print(status)
    q.put(bytes(indata))

stream = sd.RawInputStream(samplerate=16000, blocksize=8000, dtype='int16',
                           channels=1, callback=callback)
print("🎙️ Listening for speech...")

with stream:
    while True:
        data = q.get()
        if rec.AcceptWaveform(data):
            result = json.loads(rec.Result())
            if "result" in result:
                for word in result["result"]:
                    phones = word.get("phones", [])
                    for ph in phones:
                        viseme = ph.upper()[:2]  # Simplify (e.g., "AH", "OW", "M")
                        print("▶️ Viseme:", viseme)
                        sock.sendto(viseme.encode(), (udp_ip, udp_port))
