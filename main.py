from arduino.app_utils import *
from arduino.app_bricks.web_ui import WebUI
from fastapi import Body
import requests
import threading
import time

LLAMA_URL = "http://192.168.1.8:8080/v1/chat/completions"

POLL_INTERVAL = 0.3     # seconds between sensor polls
WINDOW_DURATION = 5     # seconds to confirm a trip before deciding
COOLDOWN = 2            # small gap after a decision before resuming monitoring


def query_engine(prompt):
    payload = {"messages": [{"role": "user", "content": prompt}], "max_tokens": 500}
    # Increased timeout to 600 seconds so the local LLM has time to write long answers
    r = requests.post(LLAMA_URL, json=payload, timeout=600)
    r.raise_for_status()
    return r.json()["choices"][0]["message"]["content"].strip()


def send_to_display(text):
    CHUNK_SIZE = 40
    for i in range(0, len(text), CHUNK_SIZE):
        Bridge.call("display_chunk", text[i:i + CHUNK_SIZE])
    Bridge.call("display_done")


def parse_state(raw):
    parts = dict(item.split(":") for item in raw.split(","))
    return {k: int(v) for k, v in parts.items()}


def run_decision_engine(latched, day):
    # --- CRITICAL FIX: Python does the logic instead of trusting the LLM ---
    active_hazards = []
    if latched['fire']: active_hazards.append("a Fire")
    if latched['aerial']: active_hazards.append("an Aerial Threat")
    if latched['sound']: active_hazards.append("a Loud Acoustic Anomaly")
    
    # Join them together (e.g., "an Aerial Threat and a Loud Acoustic Anomaly")
    hazard_str = " and ".join(active_hazards) if active_hazards else "an Unknown Anomaly"

    # Now we explicitly tell the LLM exactly what happened so it cannot hallucinate
    prompt = (
        f"You are a security system. The following hazard was detected: {hazard_str}. "
        "Your action is SPRINKLE. "
        "You MUST answer in exactly ONE continuous line of text. No line breaks. "
        "Format exactly like this: "
        f"DECISION: SPRINKLE. REASON: [Write a descriptive 15 to 20 word sentence using natural language explaining that {hazard_str} triggered the system and the sprinkler was activated.]"
    )

    Bridge.call("display_thinking")
    try:
        answer = query_engine(prompt)
    except Exception as e:
        print("[Engine] request failed:", e)
        send_to_display("Service unavailable - check server connection")
        return

    decision = "SPRINKLE" if "SPRINKLE" in answer.upper() else "IDLE"
    
    # Strip out all newlines (\n) so the Arduino OLED can wrap the text without glitching
    clean_answer = answer.replace("\n", " ").replace("\r", " ").strip()
    
    # Send the cleaned, single-line string to the display
    send_to_display(clean_answer)

    if decision == "SPRINKLE":
        Bridge.call("activate_sprinkler")

    print(f"[Engine] decision={decision} | fire={latched['fire']} aerial={latched['aerial']} "
          f"sound={latched['sound']} day={day}")

def monitor_loop():
    while True:
        try:
            state = parse_state(Bridge.call("get_sensor_state"))
        except Exception as e:
            print("[Monitor] sensor read failed:", e)
            time.sleep(POLL_INTERVAL)
            continue

        if state["fire"] or state["aerial"] or state["sound"]:
            # Something tripped — open a 5s confirmation window, OR-ing readings together
            window_end = time.time() + WINDOW_DURATION
            latched = {"fire": state["fire"], "aerial": state["aerial"], "sound": state["sound"]}
            day = state["day"]

            while time.time() < window_end:
                try:
                    s2 = parse_state(Bridge.call("get_sensor_state"))
                    latched["fire"] |= s2["fire"]
                    latched["aerial"] |= s2["aerial"]
                    latched["sound"] |= s2["sound"]
                    day = s2["day"]
                except Exception as e:
                    print("[Monitor] sensor read failed during window:", e)
                time.sleep(POLL_INTERVAL)

            # Snapshot frozen — decide now (no new triggers read until this returns)
            run_decision_engine(latched, day)
            time.sleep(COOLDOWN)
        else:
            time.sleep(POLL_INTERVAL)


def handle_ask(data: dict = Body(...)):
    prompt = data.get("prompt", "").strip()
    if not prompt:
        return {"answer": ""}
    Bridge.call("display_thinking")
    
    answer = query_engine(prompt)
    
    # Sanitize newlines here too
    clean_answer = answer.replace("\n", " ").replace("\r", " ").strip()
    
    send_to_display(clean_answer)
    return {"answer": answer}


def handle_decide(data: dict = Body(...)):
    # Manual demo-safety button: force a system decision on the CURRENT instantaneous
    # sensor state, bypassing the 5s window — useful if live sensors misbehave on stage.
    state = parse_state(Bridge.call("get_sensor_state"))
    run_decision_engine(state, state["day"])
    return {"status": "triggered"}


ui = WebUI()
ui.expose_api("POST", "/ask", handle_ask)
ui.expose_api("POST", "/decide", handle_decide)

threading.Thread(target=monitor_loop, daemon=True).start()

App.run()
