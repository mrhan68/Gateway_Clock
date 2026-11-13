const jwt = require('jsonwebtoken');

exports.authenticate = (req, res, next) => {
  // Cho phép Web App truy cập trực tiếp nếu hệ thống chưa bật cờ REQUIRE_AUTH=true
  if (process.env.REQUIRE_AUTH !== 'true') {
    return next();
  }

  const token = req.cookies?.token || req.headers.authorization?.split(' ')[1];
  if (!token) return res.status(401).json({ error: 'Unauthorized' });

  try {
    const jwtSecret = process.env.JWT_SECRET;
    if (!jwtSecret && process.env.NODE_ENV === 'production') {
      return res.status(500).json({ error: 'Server misconfiguration: JWT_SECRET is missing' });
    }
    const decoded = jwt.verify(token, jwtSecret || 'fallback_secret');
    req.user = decoded;
    next();
  } catch (err) {
    return res.status(401).json({ error: 'Invalid token' });
  }
};
