const { PrismaClient } = require('@prisma/client');

function buildPrismaOptions() {
  const rawUrl = process.env.DATABASE_URL;
  if (!rawUrl || typeof rawUrl !== 'string') {
    return undefined;
  }
  try {
    const parsed = new URL(rawUrl);
    if (!parsed.searchParams.has('connection_limit')) {
      parsed.searchParams.set('connection_limit', process.env.PRISMA_CONNECTION_LIMIT || '3');
    }
    if (!parsed.searchParams.has('pool_timeout')) {
      parsed.searchParams.set('pool_timeout', process.env.PRISMA_POOL_TIMEOUT || '20');
    }
    return {
      datasources: {
        db: { url: parsed.toString() }
      }
    };
  } catch (_err) {
    return undefined;
  }
}

const opts = buildPrismaOptions();
const prisma = opts ? new PrismaClient(opts) : new PrismaClient();

module.exports = prisma;
