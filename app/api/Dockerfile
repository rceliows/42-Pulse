FROM node:20-alpine

WORKDIR /app

RUN apk add --no-cache docker-cli docker-cli-compose

COPY app/api/package*.json /app/api/
RUN cd /app/api && npm ci --omit=dev

COPY app/api/ /app/api/

EXPOSE 3000

CMD ["node", "api/server.js"]
