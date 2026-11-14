const express = require('express');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const prisma = require('../config/prisma');
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

const rateLimit = require('express-rate-limit');

// Rate limiting for auth routes: max 20 attempts per 15 minutes per IP
const authLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 20,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: 'Quá nhiều lần thử xác thực. Vui lòng thử lại sau 15 phút.' }
});

router.post('/login', authLimiter, async (req, res) => {
  const { username, password } = req.body;
  if (!username || !password) {
    return res.status(400).json({ error: 'Yêu cầu tên đăng nhập và mật khẩu' });
  }

  const user = await prisma.user.findUnique({ where: { username } });
  if (!user || !verifyPassword(user.passwordHash, password)) {
    return res.status(401).json({ error: 'Tên đăng nhập hoặc mật khẩu không chính xác' });
  }

  const jwtSecret = process.env.JWT_SECRET;
  if (!jwtSecret) {
    if (process.env.NODE_ENV === 'production') {
      console.error('[FATAL SECURITY ERROR] JWT_SECRET must be configured in production!');
      return res.status(500).json({ error: 'Server misconfiguration: JWT_SECRET environment variable is missing' });
    }
  }

  const secretToUse = jwtSecret || 'fallback_secret';
  const token = jwt.sign(
    { id: user.id, username: user.username },
    secretToUse,
    { expiresIn: process.env.JWT_EXPIRES_IN || '24h' }
  );
  res.cookie('token', token, { httpOnly: true, sameSite: 'strict', maxAge: 24 * 60 * 60 * 1000 });
  res.json({
    access_token: token,
    user: {
      id: user.id,
      username: user.username,
      email: user.email || null
    }
  });
});

router.post('/register', authLimiter, async (req, res) => {
  const { username, password, email } = req.body;
  if (!username || !password) {
    return res.status(400).json({ error: 'Yêu cầu tên đăng nhập và mật khẩu' });
  }
  if (username.length < 3) {
    return res.status(400).json({ error: 'Tên đăng nhập tối thiểu 3 ký tự' });
  }
  if (password.length < 6) {
    return res.status(400).json({ error: 'Mật khẩu tối thiểu 6 ký tự' });
  }

  try {
    const existing = await prisma.user.findUnique({ where: { username } });
    if (existing) {
      return res.status(409).json({ error: 'Tên đăng nhập đã tồn tại' });
    }

    const passwordHash = hashPassword(password);
    const user = await prisma.user.create({
      data: {
        username: username.trim(),
        email: email ? email.trim() : null,
        passwordHash
      }
    });

    const jwtSecret = process.env.JWT_SECRET;
    if (!jwtSecret && process.env.NODE_ENV === 'production') {
      return res.status(500).json({ error: 'Server misconfiguration: JWT_SECRET environment variable is missing' });
    }
    const token = jwt.sign(
      { id: user.id, username: user.username },
      jwtSecret || 'fallback_secret',
      { expiresIn: process.env.JWT_EXPIRES_IN || '24h' }
    );
    res.cookie('token', token, { httpOnly: true, sameSite: 'strict', maxAge: 24 * 60 * 60 * 1000 });
    return res.status(201).json({
      access_token: token,
      user: {
        id: user.id,
        username: user.username,
        email: user.email || null
      }
    });
  } catch (err) {
    return res.status(500).json({ error: 'Đăng ký thất bại: ' + err.message });
  }
});

router.get('/me', async (req, res) => {
  const token = req.cookies?.token || req.headers.authorization?.split(' ')[1];
  if (!token) {
    return res.status(401).json({ error: 'Chưa xác thực' });
  }

  try {
    const jwtSecret = process.env.JWT_SECRET;
    if (!jwtSecret && process.env.NODE_ENV === 'production') {
      return res.status(500).json({ error: 'Server misconfiguration: JWT_SECRET environment variable is missing' });
    }
    const decoded = jwt.verify(token, jwtSecret || 'fallback_secret');
    const user = await prisma.user.findUnique({
      where: { id: decoded.id },
      select: { id: true, username: true, email: true, createdAt: true }
    });
    if (!user) {
      return res.status(401).json({ error: 'Không tìm thấy người dùng' });
    }
    return res.json({ user });
  } catch {
    return res.status(401).json({ error: 'Token không hợp lệ hoặc đã hết hạn' });
  }
});

module.exports = router;
module.exports.verifyPassword = verifyPassword;
module.exports.hashPassword = hashPassword;
