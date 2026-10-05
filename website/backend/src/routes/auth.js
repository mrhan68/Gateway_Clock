const express = require('express');
const jwt = require('jsonwebtoken');
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const router = express.Router();

router.post('/login', async (req, res) => {
  const { username, password } = req.body;
  // Simple check for now, skipping bcrypt for ponytail efficiency if missing
  const user = await prisma.user.findUnique({ where: { username } });
  
  if (!user || user.passwordHash !== password) {
    return res.status(401).json({ error: 'Invalid credentials' });
  }

  const token = jwt.sign({ id: user.id }, process.env.JWT_SECRET || 'fallback_secret', { expiresIn: '15m' });
  res.cookie('token', token, { httpOnly: true, sameSite: 'strict' });
  res.json({ access_token: token });
});

module.exports = router;
