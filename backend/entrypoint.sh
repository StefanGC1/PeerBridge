#!/usr/bin/env sh

# Initialize migration environment
if [ ! -d migrations ]; then
  echo "Initializing migrations directory..."
  flask db init
fi

# Generate new migration scripts
echo "Running flask db migrate..."
flask db migrate -m "autogenerate migrations" || true

# Apply any migrations
echo "Upgrading database..."
flask db upgrade

# Start backend
exec python run.py