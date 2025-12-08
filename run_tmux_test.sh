#!/bin/bash

SESSION_NAME="esp32_qemu_test"
LOG_FILE="$(pwd)/log.txt"
PROJECT_DIR="examples/esp32_ubpf_demo"

# Ensure no previous session exists
tmux kill-session -t $SESSION_NAME 2>/dev/null

# 1. Spawn a tmux window (detached session)
echo "Spawning tmux session: $SESSION_NAME"
tmux new-session -d -s $SESSION_NAME

# 2. Enable logging of the pane to log.txt
echo "Logging to: $LOG_FILE"
# Clear existing log
> "$LOG_FILE"
tmux pipe-pane -t ${SESSION_NAME}:0 -o "cat >> $LOG_FILE"

# 3. Run commands
echo "Running 'idf.py qemu monitor' in $PROJECT_DIR..."
tmux send-keys -t $SESSION_NAME "cd $PROJECT_DIR" C-m
tmux send-keys -t $SESSION_NAME "idf.py qemu monitor" C-m

# 4. Wait for execution (adjust timeout as needed)
echo "Waiting for 45 seconds..."
sleep 45

# 5. Kill the window/session
echo "Killing tmux session..."
tmux kill-session -t $SESSION_NAME

echo "Done. Check $LOG_FILE for output."
