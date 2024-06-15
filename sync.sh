#!/bin/bash
# Configuration
SERVER_USER=root
SERVER_IP=185.157.247.62
LOCAL_FOLDER="$(pwd)"
REMOTE_FOLDER="/project/$(eval whoami)/"

# Initial environment settings
TERM=xterm-256color

# Flag to control syncing behavior
SYNC_ENABLED=true

# Function to perform sync from local to remote
sync_local_to_remote() {
  if $SYNC_ENABLED; then
    # Ensure the remote directory exists
    ssh "$SERVER_USER@$SERVER_IP" "mkdir -p $REMOTE_FOLDER"
    # Proceed with rsync
    rsync -avz --delete --exclude='*.o' --rsync-path="env -i TERM=$TERM /usr/bin/rsync" "$LOCAL_FOLDER/" "$SERVER_USER@$SERVER_IP:$REMOTE_FOLDER"
    echo "Auto-pushed due to local changes."
  fi
}

# Function to perform sync from remote to local
sync_remote_to_local() {
  SYNC_ENABLED=false
  rsync -avz --delete --exclude='*.o' --rsync-path="env -i TERM=$TERM /usr/bin/rsync" "$SERVER_USER@$SERVER_IP:$REMOTE_FOLDER/" "$LOCAL_FOLDER" > /dev/null
  echo "Pulled remote changes to local."
  SYNC_ENABLED=true
}

trap 'kill $FSWATCH_PID; exit' SIGINT

# Start monitoring local changes and syncing to remote in the background
fswatch -o "$LOCAL_FOLDER" | while read f
do
  sync_local_to_remote
done &
FSWATCH_PID=$!

# Initial sync from local to remote
sync_local_to_remote

# Handling user commands for manual push or pull
echo -n "Enter 'push' to sync local to remote, 'pull' to sync remote to local"
while IFS= read -r user_command; do
  case $user_command in
    push)
      echo "Pushing local changes to remote..."
      sync_local_to_remote
      ;;
    pull)
      echo "Pulling remote changes to local..."
      sync_remote_to_local
      ;;
    # Removed the exit option
    *)
      echo "Invalid command. Use 'push', 'pull', or 'exit'."
      ;;
  esac
  echo -n "Enter 'push' to sync local to remote, 'pull' to sync remote to local: "
done
