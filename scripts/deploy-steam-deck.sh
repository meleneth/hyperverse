#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUNDLE_DIR=${BUNDLE_DIR:-$PROJECT_DIR/dist/hyperverse-steamrt}
REMOTE_DIR=${REMOTE_DIR:-/home/deck/Games/hyperverse}
SSH_IDENTITY=${SSH_IDENTITY:-${HOME:?}/.ssh/hyperverse_deck_ed25519}

if [ -f "$SSH_IDENTITY" ]; then
  SSH_COMMAND="ssh -i $SSH_IDENTITY -o IdentitiesOnly=yes"
else
  SSH_COMMAND=ssh
fi

usage() {
  cat <<'EOF'
Usage: ./scripts/deploy-steam-deck.sh deck@HOST

Copies the already-built Steam Runtime bundle to the Deck over SSH. The
destination defaults to /home/deck/Games/hyperverse and is synchronized with
--delete so removed bundle files are also removed from that directory.

Environment overrides:
  BUNDLE_DIR  Local bundle. Default: dist/hyperverse-steamrt
  REMOTE_DIR  Deck destination. Default: /home/deck/Games/hyperverse
  SSH_IDENTITY  Deployment key. Default: ~/.ssh/hyperverse_deck_ed25519
EOF
}

if [ "$#" -ne 1 ]; then
  usage >&2
  exit 2
fi

TARGET=$1
case "$TARGET" in
  deck@*) ;;
  *)
    echo "Target must use the Deck account, for example deck@192.168.1.50" >&2
    exit 2
    ;;
esac

case "$REMOTE_DIR" in
  /home/deck/Games/hyperverse | /home/deck/Games/hyperverse/*) ;;
  *)
    echo "Refusing unsafe REMOTE_DIR: $REMOTE_DIR" >&2
    exit 2
    ;;
esac

for command_name in ssh rsync; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "$command_name is required for Deck deployment." >&2
    exit 1
  fi
done

if [ ! -x "$BUNDLE_DIR/run-hyperverse.sh" ]; then
  echo "No built bundle found at $BUNDLE_DIR" >&2
  echo "Run ./scripts/build-steam-runtime.sh first." >&2
  exit 1
fi

if ! $SSH_COMMAND "$TARGET" true; then
  cat >&2 <<EOF
Could not log into $TARGET over SSH.

On the Deck in Desktop Mode, open Konsole and run:
  passwd
  sudo systemctl enable --now sshd
  hostname -I

Then test from this machine with:
  ssh $TARGET
EOF
  exit 1
fi

$SSH_COMMAND "$TARGET" "mkdir -p '$REMOTE_DIR'"
rsync --rsh="$SSH_COMMAND" --archive --compress --delete --human-readable --progress \
  "$BUNDLE_DIR/" "$TARGET:$REMOTE_DIR/"

cat <<EOF
Hyperverse deployed to $TARGET:$REMOTE_DIR

Steam shortcut target:
  $REMOTE_DIR/run-hyperverse.sh
Start In:
  $REMOTE_DIR
EOF
