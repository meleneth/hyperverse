#!/usr/bin/env bash
set -euo pipefail

speedster_uuid="34308C4F308C1A4E"
rikitikitavi_uuid="B4B7-C399"
speedster_mount="/mnt/speedster"
rikitikitavi_mount="/mnt/rikitikitavi"

if [[ ${EUID} -eq 0 ]]; then
  invoking_uid="${SUDO_UID:-0}"
  invoking_gid="${SUDO_GID:-0}"
else
  invoking_uid="$(id -u)"
  invoking_gid="$(id -g)"
fi

if [[ ${invoking_uid} -eq 0 ]]; then
  echo "Run this as your regular desktop user, not directly as root." >&2
  exit 1
fi

if ! command -v blkid >/dev/null || ! command -v findmnt >/dev/null; then
  echo "This script requires blkid and findmnt (normally provided by util-linux)." >&2
  exit 1
fi

if ! blkid -U "${speedster_uuid}" >/dev/null 2>&1; then
  echo "Speedster (UUID=${speedster_uuid}) is not currently connected." >&2
  exit 1
fi

tmp_fstab="$(mktemp --tmpdir setup-speedster-fstab.XXXXXX)"
cleanup() {
  rm -f -- "${tmp_fstab}"
}
trap cleanup EXIT

awk -v speedster_uuid="${speedster_uuid}" \
    -v rikitikitavi_uuid="${rikitikitavi_uuid}" '
  $1 == "UUID=" speedster_uuid { next }
  $1 == "UUID=" rikitikitavi_uuid { next }
  { print }
' /etc/fstab >"${tmp_fstab}"

printf '%s\n' \
  "UUID=${speedster_uuid} ${speedster_mount} ntfs3 rw,user,noauto,uid=${invoking_uid},gid=${invoking_gid},umask=0022,windows_names,nofail,x-systemd.device-timeout=10 0 0" \
  "UUID=${rikitikitavi_uuid} ${rikitikitavi_mount} exfat rw,user,noauto,uid=${invoking_uid},gid=${invoking_gid},umask=0022,nofail,x-systemd.device-timeout=10 0 0" \
  >>"${tmp_fstab}"

if [[ "$(awk 'NF && $1 !~ /^#/ && NF != 6 { count++ } END { print count + 0 }' "${tmp_fstab}")" -ne 0 ]]; then
  echo "Refusing to install an invalid fstab." >&2
  exit 1
fi

echo "Requesting sudo to install the mount configuration..."
sudo -v

backup="/etc/fstab.before-speedster-$(date +%Y%m%d-%H%M%S)"
sudo cp --preserve=all /etc/fstab "${backup}"
sudo mkdir -p "${speedster_mount}" "${rikitikitavi_mount}"
sudo install -o root -g root -m 0644 "${tmp_fstab}" /etc/fstab
sudo systemctl daemon-reload

echo "Installed /etc/fstab (backup: ${backup})"
echo "Testing Speedster mount and unmount without sudo..."

if findmnt --target "${speedster_mount}" >/dev/null 2>&1; then
  umount "${speedster_mount}"
fi
mount "${speedster_mount}"
findmnt --target "${speedster_mount}" --output SOURCE,TARGET,FSTYPE,OPTIONS
umount "${speedster_mount}"

echo
echo "Success. Either filesystem can now be managed without sudo:"
echo "  mount ${speedster_mount}"
echo "  umount ${speedster_mount}"
echo "  mount ${rikitikitavi_mount}"
echo "  umount ${rikitikitavi_mount}"
