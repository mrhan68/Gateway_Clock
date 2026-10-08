const express = require('express');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const router = express.Router();

function verifyPassword(stored, provided) {
  if (!stored || !provided) return false;
  // Format salt:scrypt_hash (preferred secure salted hash)
  if (stored.includes(':')) {
    const [salt, key] = stored.split(':');
    if (salt && key) {
      try {
        const derived = crypto.scryptSync(provided, salt, 64);
        const keyBuf = Buffer.from(key, 'hex');
        if (derived.length === keyBuf.length) {
          return crypto.timingSafeEqual(derived, keyBuf);
        }
      } catch {
        return false;
      }
    }
  }
  // Format SHA-256 (64 hex characters)
  if (stored.length === 64 && /^[0-9a-fA-F]+$/.test(stored)) {
    const hash = crypto.createHash('sha256').update(provided).digest('hex');
    return crypto.timingSafeEqual(Buffer.from(stored, 'hex'), Buffer.from(hash, 'hex'));
  }
  // Constant-time fallback for legacy strings
  const bStored = Buffer.from(stored);
  const bProvided = Buffer.from(provided);
  if (bStored.length !== bProvided.length) return false;
  return crypto.timingSafeEqual(bStored, bProvided);
}

function hashPassword(password) {
  const salt = crypto.randomBytes(16).toString('hex');
  const derived = crypto.scryptSync(password, salt, 64);
  return `${salt}:${derived.toString('hex')}`;
}

router.post('/login', async (req, res) => {
  const { username, password } = req.body;
  if (!username || !password) {
    return res.status(400).json({ error: 'Username and password required' });
  }

  const user = await prisma.user.findUnique({ where: { username } });
  if (!user || !verifyPassword(user.passwordHash, password)) {
    return res.status(401).json({ error: 'Invalid credentials' });
  }

  const jwtSecret = process.env.JWT_SECRET;
  if (!jwtSecret) {
    if (process.env.NODE_ENV === 'production') {
      console.error('[FATAL SECURITY ERROR] JWT_SECRET must be configured in production!');
      return res.status(500).json({ error: 'Server misconfiguration: JWT_SECRET environment variable is missing' });
    }
  }

  const secretToUse = jwtSecret || 'fallback_secret';
  const token = jwt.sign({ id: user.id }, secretToUse, { expiresIn: '15m' });
  res.cookie('token', token, { httpOnly: true, sameSite: 'strict' });
  res.json({ access_token: token });
});

module.exports = router;
module.exports.verifyPassword = verifyPassword;
module.exports.hashPassword = hashPassword;
