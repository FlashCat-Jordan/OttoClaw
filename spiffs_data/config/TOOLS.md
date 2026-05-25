# Tool Documentation

## Available Tools

### web_search
Search the web for current information.

**MUST use for:**
- News, weather, prices, stock, scores, exchange rates — any real-time data
- Events, people, products, companies you are not 100% certain about
- Anything that may have changed since your training cutoff
- User says: 搜/查/找/最新/now/latest/search/look up
- When uncertain whether info is current — ALWAYS search first

**NEVER use for:**
- Pure math, logic, coding, definitions
- Casual chitchat, greetings, personal opinions
- Questions about yourself or the robot hardware

### get_current_time
Get the current date and time.
You do NOT have an internal clock — always use this tool when you need to know the time or date.

### read_file
Read a file from SPIFFS (path must start with /spiffs/).

### write_file
Write/overwrite a file on SPIFFS.

### edit_file
Find-and-replace edit a file on SPIFFS.

### list_dir
List files on SPIFFS, optionally filter by prefix.

### memory_write
Write to long-term memory storage.

### memory_append_today
Append a note to today's daily log.

### self.otto.action
Execute a predefined robot action.
Parameters:
- action: Predefined name (walk/turn/jump/swing/moonwalk/bend/shake_leg/sit/updown/hands_up/hands_down/hand_wave/windmill/takeoff/fitness/greeting/shy/radio_calisthenics/magic_circle/showcase/home)
- steps: Repeat count 1-100, default 3
- speed: Speed 100-3000 (lower=faster), default 1000
- direction: 1=forward/left, -1=backward/right, 0=both hands
- amount: Range 0-170, default 25

### self.otto.pose (Servo Sequences Lite)
Directly control each servo joint to reach a specific angle. Use this for **improvised creative actions** — any gesture, pose, or movement sequence the AI imagines.

Parameters (all optional, default to neutral position):
- left_leg: Left leg servo angle 0-180 (90=neutral, 0/180=extreme tilt)
- right_leg: Right leg servo angle 0-180 (90=neutral)
- left_foot: Left foot servo angle 0-180 (90=neutral, low=tilt left, high=tilt right)
- right_foot: Right foot servo angle 0-180 (90=neutral)
- left_hand: Left hand servo angle 0-180 (45=rest, 0=arm down, 170=arm up high)
- right_hand: Right hand servo angle 0-180 (135=rest, 180=arm down, 10=arm up high)
- time: Transition time in ms 100-3000 (default 700). Shorter=faster snap, longer=smooth slow motion.

**How to improvise actions with pose:**
When a user asks for a creative, improvised, or novel action (求婚/单膝跪地/拥抱/祈祷/跳舞姿势/情绪表达), think about what body posture expresses that action, then call pose with the angles that create it. You can chain multiple pose calls to create a sequence:
1. Think: what posture expresses this? (e.g., 求婚 = right leg bent low + right hand raised high + head tilt)
2. Call pose with those angles
3. Hold for a moment, then optionally call pose again to transition back to neutral (all 90/45/135)
4. Optionally add an emotion text response

**Angle reference:**
- Legs: 90=standing straight, 0/180=extreme lean, ~30=tilted sideways, ~120=leaning other way
- Feet: 90=flat, 0=extreme tilt left, 180=extreme tilt right, ~30=gently tilted
- Left hand: 45=natural rest, 160-170=raised high, 0-20=lowered
- Right hand: 135=natural rest, 10-20=raised high, 160-180=lowered
