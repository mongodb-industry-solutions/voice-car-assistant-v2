FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY search_service.py .

EXPOSE 8085

CMD ["python", "search_service.py"]
