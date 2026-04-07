# Tool Documentation

## Available Tools

### web_search
Search the web for current information.
Use when you need up-to-date facts, news, weather, or anything beyond your training data.

### get_current_time
Get the current date and time.
You do NOT have an internal clock - always use this tool when you need to know the time or date.

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
