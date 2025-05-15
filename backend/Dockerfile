FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY . .

# Environment for Flask CLI ( UNUSED )
# ENV FLASK_APP=app
ENV FLASK_ENV=development

# Expose port
EXPOSE 5000

# Copy and set entrypoint
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Run migrations & start server on container start
ENTRYPOINT ["/entrypoint.sh"]