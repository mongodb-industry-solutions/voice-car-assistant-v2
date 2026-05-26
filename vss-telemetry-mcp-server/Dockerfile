FROM node:20-alpine
WORKDIR /app
COPY package.json .
RUN npm install --production
COPY . .
RUN chown -R node:node /app
USER node
EXPOSE 3002
HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD wget -q --spider http://localhost:3002/health || exit 1
CMD ["node", "server.js"]
