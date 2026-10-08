const express = require('express');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const router = express.Router();

function verifyPassword(stored, provided) {
  if (!stored || !provided) return false;
  if (stored.length === 64 && /^[0-9a-fA-F]+$/.test(stored)) {
    const hash = crypto.createHash('sha256').update(provided).digest('hex');
    return crypto.timingSafeEqual(Buffer.from(stored, 'hex'), Buffer.from(hash, 'hex'));
  }
  const bStored = Buffer.from(stored);
  const bProvided = Buffer.from(provided);
  if (bStored.length !== bProvided.length) return false;
  return crypto.timingSafeEqual(bStored, bProvided);
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

  const jwtSecret = process.env.JWT_SECRET || 'fallback_secret';
  if (process.env.NODE_ENV === 'production' && jwtSecret === 'fallback_secret') {
    console.warn('[SECURITY WARNING] Using fallback JWT secret in production! Set JWT_SECRET in environment.');
  }

  const token = jwt.sign({ id: user.id }, jwtSecret, { expiresIn: '15m' });
  res.cookie('token', token, { httpOnly: true, sameSite: 'strict' });
  res.json({ access_token: token });
});

module.exports = router;
