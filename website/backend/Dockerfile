FROM node:20-alpine AS base
WORKDIR /app
# Install native dependencies for Prisma
RUN apk add --no-cache libc6-compat openssl
COPY package.json package-lock.json* ./
RUN npm ci --only=production

FROM base AS builder
# Install all deps including dev deps for Prisma generation
RUN npm ci
COPY prisma ./prisma/
COPY . .
RUN npx prisma generate

FROM node:20-alpine AS runtime
WORKDIR /app
ENV NODE_ENV=production
# Copy node_modules (production deps only)
COPY --from=base /app/node_modules ./node_modules
# Copy app source and prisma client
COPY --from=builder /app/prisma ./prisma/
COPY --from=builder /app/node_modules/.prisma/client ./node_modules/.prisma/client
COPY . .
# Prisma needs generated client at runtime
RUN npx prisma generate --data-proxy

EXPOSE 3000 1883
HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:3000/health || exit 1

CMD ["npm", "start"]
