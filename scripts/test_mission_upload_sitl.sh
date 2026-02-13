#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${PX4_SITL_UDP:-}" ]]; then
  echo "[SKIP] PX4_SITL_UDP not set (e.g. 14550)" >&2
  exit 0
fi

MISSION_FILE=${1:-config/sample_mission.json}

echo "[INFO] Running GCS with mission file: ${MISSION_FILE}"
./my_gcs "${MISSION_FILE}" | tee /tmp/gcs_mission_upload.log

echo "[INFO] Check /tmp/gcs_mission_upload.log for MISSION_COUNT/REQUEST/ACK lines"