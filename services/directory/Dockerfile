# syntax=docker/dockerfile:1

FROM node:24-trixie-slim AS dependencies
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci --omit=dev

FROM dependencies AS verification
USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends openjdk-21-jre-headless \
    && rm -rf /var/lib/apt/lists/* \
    && npm install --global firebase-tools@15.31.0
COPY --chown=node:node . .
RUN chown node:node /app
USER node
ENV CI=true \
    NODE_ENV=test \
    GOOGLE_CLOUD_PROJECT=demo-duel6r-directory \
    FIRESTORE_EMULATOR_HOST=127.0.0.1:8080 \
    PORT=8081
RUN firebase setup:emulators:firestore
ENTRYPOINT ["firebase", "emulators:exec", "--only", "firestore", "--project", "demo-duel6r-directory"]
CMD ["npm test"]

FROM dependencies AS production
COPY --chown=node:node src ./src
USER node
ENV NODE_ENV=production PORT=8080
EXPOSE 8080
CMD ["node", "src/main.js"]
