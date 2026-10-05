import os

with open('website/backend/src/app.js', 'r') as f:
    code = f.read()

# Fix CORS
code = code.replace(
    'origin: true, // Allow all origins in dev; configure for prod',
    "origin: process.env.ALLOWED_ORIGIN || 'http://localhost:3000',"
)

# Insert cookie-parser and auth middleware
code = code.replace(
    "const express = require('express');",
    "const express = require('express');\nconst cookieParser = require('cookie-parser');\nconst { authenticate } = require('./middleware/auth');"
)
code = code.replace(
    "app.use(express.urlencoded({ extended: true, limit: '10mb' }));",
    "app.use(express.urlencoded({ extended: true, limit: '10mb' }));\napp.use(cookieParser());\n\n// Auth routes\napp.use('/auth', require('./routes/auth'));"
)

# Protect API routes
code = code.replace("app.use('/api/v1/gateways'", "app.use('/api/v1/gateways', authenticate")
code = code.replace("app.use('/api/v1/devices'", "app.use('/api/v1/devices', authenticate")
code = code.replace("app.use('/api/v1/schedules'", "app.use('/api/v1/schedules', authenticate")
code = code.replace("app.use('/api/v1/rules'", "app.use('/api/v1/rules', authenticate")
code = code.replace("app.use('/api/v1/events'", "app.use('/api/v1/events', authenticate")
code = code.replace("app.use('/api/v1/telemetry'", "app.use('/api/v1/telemetry', authenticate")

with open('website/backend/src/app.js', 'w') as f:
    f.write(code)
