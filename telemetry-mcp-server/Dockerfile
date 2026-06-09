FROM node:20-slim

RUN apt-get update && apt-get install -y --no-install-recommends wget && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY package*.json ./
RUN npm install --omit=dev

COPY . .

# node:20-slim ships a built-in 'node' user (UID 1000); transfer ownership and drop privileges.
RUN chown -R node:node /app
USER node

EXPOSE 3001

ENV MCP_TRANSPORT=sse
ENV PORT=3001

CMD ["node", "server.js"]
