const { verifyPassword, hashPassword } = require('../src/routes/auth');
const { authenticate } = require('../src/middleware/auth');
const jwt = require('jsonwebtoken');

describe('Authentication & Security Unit Tests', () => {
  describe('Password Hashing & Verification (scrypt)', () => {
    test('hashes password with random salt and successfully verifies match', () => {
      const rawPassword = 'SecurePassword123!';
      const hash = hashPassword(rawPassword);

      expect(hash).toContain(':');
      expect(verifyPassword(hash, rawPassword)).toBe(true);
    });

    test('rejects incorrect password with constant-time check', () => {
      const rawPassword = 'SecurePassword123!';
      const hash = hashPassword(rawPassword);

      expect(verifyPassword(hash, 'WrongPassword')).toBe(false);
      expect(verifyPassword(hash, '')).toBe(false);
      expect(verifyPassword(null, rawPassword)).toBe(false);
    });
  });

  describe('JWT Authentication Middleware', () => {
    const originalEnv = process.env.REQUIRE_AUTH;
    const secret = 'test_jwt_secret_key_123456';

    beforeAll(() => {
      process.env.JWT_SECRET = secret;
    });

    afterAll(() => {
      process.env.REQUIRE_AUTH = originalEnv;
    });

    test('bypasses authentication when REQUIRE_AUTH is not true', () => {
      process.env.REQUIRE_AUTH = 'false';
      const req = { headers: {} };
      const res = { status: jest.fn().mockReturnThis(), json: jest.fn() };
      const next = jest.fn();

      authenticate(req, res, next);
      expect(next).toHaveBeenCalled();
      expect(res.status).not.toHaveBeenCalled();
    });

    test('rejects request with 401 when REQUIRE_AUTH=true and token is missing', () => {
      process.env.REQUIRE_AUTH = 'true';
      const req = { headers: {} };
      const res = { status: jest.fn().mockReturnThis(), json: jest.fn() };
      const next = jest.fn();

      authenticate(req, res, next);
      expect(next).not.toHaveBeenCalled();
      expect(res.status).toHaveBeenCalledWith(401);
      expect(res.json).toHaveBeenCalledWith(expect.objectContaining({ error: expect.any(String) }));
    });

    test('rejects request with 401 when token is invalid or expired', () => {
      process.env.REQUIRE_AUTH = 'true';
      const req = { headers: { authorization: 'Bearer invalid_garbage_token' } };
      const res = { status: jest.fn().mockReturnThis(), json: jest.fn() };
      const next = jest.fn();

      authenticate(req, res, next);
      expect(next).not.toHaveBeenCalled();
      expect(res.status).toHaveBeenCalledWith(401);
    });

    test('allows request and populates req.user when valid Bearer token provided', () => {
      process.env.REQUIRE_AUTH = 'true';
      const payload = { id: 1, username: 'admin' };
      const validToken = jwt.sign(payload, secret, { expiresIn: '1h' });

      const req = { headers: { authorization: `Bearer ${validToken}` } };
      const res = { status: jest.fn().mockReturnThis(), json: jest.fn() };
      const next = jest.fn();

      authenticate(req, res, next);
      expect(next).toHaveBeenCalled();
      expect(req.user).toBeDefined();
      expect(req.user.id).toBe(1);
      expect(req.user.username).toBe('admin');
    });
  });
});
