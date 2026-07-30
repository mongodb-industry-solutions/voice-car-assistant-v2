# Next.js cockpit UI — standalone output, served on :8080
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
RUN npm run build

FROM node:22-alpine
WORKDIR /app
ENV NODE_ENV=production
ENV PORT=8080
# Standalone bundle (server.js + minimal node_modules), static assets, and public/.
# Files are owned by the built-in non-root `node` user (uid 1000) so the server
# does not run as root.
COPY --from=builder --chown=node:node /app/public ./public
COPY --from=builder --chown=node:node /app/.next/standalone ./
COPY --from=builder --chown=node:node /app/.next/static ./.next/static

USER node

EXPOSE 8080
# The chart health probe hits "/", which Next serves.
CMD ["node", "server.js"]
