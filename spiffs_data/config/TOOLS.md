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
Control the Otto robot's movements and expressions.
Parameters:
- action: The action name (e.g., "wave", "jump", "shake")
