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
COPY --from=builder /app/public ./public
COPY --from=builder /app/.next/standalone ./
COPY --from=builder /app/.next/static ./.next/static

EXPOSE 8080
# The chart health probe hits "/", which Next serves.
CMD ["node", "server.js"]
