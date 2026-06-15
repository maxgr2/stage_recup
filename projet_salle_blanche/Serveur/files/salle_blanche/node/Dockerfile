# Image ARM compatible Raspberry Pi 3B+
FROM node:18-alpine

# Création d'un utilisateur non-root
RUN addgroup -S appgroup && adduser -S appuser -G appgroup

WORKDIR /app

# Copie des dépendances en premier (cache Docker)
COPY package.json ./
RUN npm install --production

# Copie du code
COPY server.js ./

# Dossier data pour SQLite (volume monté)
RUN mkdir -p /app/data && chown appuser:appgroup /app/data

USER appuser

EXPOSE 3000

CMD ["node", "server.js"]
