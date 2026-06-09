#!/bin/bash
set -e
chown -R appuser:appuser /app/conversation-db
exec gosu appuser "$@"
