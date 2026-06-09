#!/bin/bash
set -e
chown -R appuser:appuser /app/telemetry-db
exec gosu appuser "$@"
