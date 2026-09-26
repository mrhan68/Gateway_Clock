-- Smart Clock Gateway Database Schema for Aiven PostgreSQL
-- Execute this script directly in TablePlus connected to your Aiven database

-- 1. Create Enums
DO $$ BEGIN
    CREATE TYPE "GatewayStatus" AS ENUM ('online', 'offline', 'unclaimed', 'active');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    CREATE TYPE "DeviceType" AS ENUM ('sensor', 'actuator');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    CREATE TYPE "Protocol" AS ENUM ('ble', 'mqtt');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    CREATE TYPE "DeviceStatus" AS ENUM ('online', 'offline', 'unknown');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    CREATE TYPE "ScheduleType" AS ENUM ('automation', 'alarm', 'personal_event');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    CREATE TYPE "ConditionType" AS ENUM ('gt', 'lt', 'eq');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- 2. Create Tables
CREATE TABLE IF NOT EXISTS "User" (
    "id" SERIAL NOT NULL,
    "username" VARCHAR(64) NOT NULL,
    "email" VARCHAR(128),
    "passwordHash" VARCHAR(256) NOT NULL,
    "createdAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "User_pkey" PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS "Gateway" (
    "id" VARCHAR(32) NOT NULL,
    "userId" INTEGER,
    "name" VARCHAR(64) NOT NULL,
    "status" "GatewayStatus" NOT NULL DEFAULT 'offline',
    "lastSeen" TIMESTAMP(6),
    "firmwareVersion" VARCHAR(16),
    "timezone" VARCHAR(64),
    "claimCode" VARCHAR(16),
    "claimExpiresAt" TIMESTAMP(6),
    "macAddress" VARCHAR(18),
    "createdAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "Gateway_pkey" PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS "Device" (
    "id" SERIAL NOT NULL,
    "deviceId" VARCHAR(32) NOT NULL,
    "gatewayId" TEXT NOT NULL,
    "name" VARCHAR(64) NOT NULL,
    "deviceType" "DeviceType" NOT NULL,
    "protocol" "Protocol" NOT NULL,
    "macAddress" VARCHAR(18),
    "mqttTopic" VARCHAR(128),
    "capabilities" TEXT,
    "status" "DeviceStatus" NOT NULL DEFAULT 'unknown',
    "lastSeen" TIMESTAMP(6),
    "lastValue" DOUBLE PRECISION,
    "createdAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "Device_pkey" PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS "Schedule" (
    "id" SERIAL NOT NULL,
    "gatewayId" TEXT NOT NULL,
    "type" "ScheduleType" NOT NULL,
    "hour" SMALLINT NOT NULL,
    "minute" SMALLINT NOT NULL,
    "repeatDays" VARCHAR(32),
    "oneTimeDate" DATE,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    "targetDeviceId" VARCHAR(32),
    "actionCommand" VARCHAR(32),
    "title" VARCHAR(64),
    "buzzer" BOOLEAN NOT NULL DEFAULT false,
    "createdAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "Schedule_pkey" PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS "AutomationRule" (
    "id" SERIAL NOT NULL,
    "gatewayId" TEXT NOT NULL,
    "sensorDeviceId" VARCHAR(32) NOT NULL,
    "conditionType" "ConditionType" NOT NULL,
    "threshold" DOUBLE PRECISION NOT NULL,
    "targetDeviceId" VARCHAR(32) NOT NULL,
    "actionCommand" VARCHAR(32) NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "AutomationRule_pkey" PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS "EventLog" (
    "id" SERIAL NOT NULL,
    "gatewayId" TEXT NOT NULL,
    "eventType" VARCHAR(32) NOT NULL,
    "deviceId" VARCHAR(32),
    "payload" TEXT,
    "occurredAt" TIMESTAMP(6) NOT NULL,
    "syncedAt" TIMESTAMP(6),
    CONSTRAINT "EventLog_pkey" PRIMARY KEY ("id")
);

CREATE TABLE IF NOT EXISTS "TelemetryLog" (
    "id" SERIAL NOT NULL,
    "deviceId" VARCHAR(32) NOT NULL,
    "gatewayId" TEXT NOT NULL,
    "value" DOUBLE PRECISION NOT NULL,
    "unit" VARCHAR(16),
    "recordedAt" TIMESTAMP(6) NOT NULL,
    CONSTRAINT "TelemetryLog_pkey" PRIMARY KEY ("id")
);

-- 3. Create Indexes
CREATE UNIQUE INDEX IF NOT EXISTS "User_username_key" ON "User"("username");
CREATE UNIQUE INDEX IF NOT EXISTS "User_email_key" ON "User"("email");
CREATE INDEX IF NOT EXISTS "Gateway_userId_idx" ON "Gateway"("userId");
CREATE INDEX IF NOT EXISTS "Gateway_status_idx" ON "Gateway"("status");
CREATE INDEX IF NOT EXISTS "Gateway_claimCode_idx" ON "Gateway"("claimCode");
CREATE INDEX IF NOT EXISTS "Device_gatewayId_idx" ON "Device"("gatewayId");
CREATE INDEX IF NOT EXISTS "Device_status_idx" ON "Device"("status");
CREATE UNIQUE INDEX IF NOT EXISTS "Device_gatewayId_deviceId_key" ON "Device"("gatewayId", "deviceId");
CREATE INDEX IF NOT EXISTS "Schedule_gatewayId_idx" ON "Schedule"("gatewayId");
CREATE INDEX IF NOT EXISTS "Schedule_type_idx" ON "Schedule"("type");
CREATE INDEX IF NOT EXISTS "AutomationRule_gatewayId_idx" ON "AutomationRule"("gatewayId");
CREATE INDEX IF NOT EXISTS "AutomationRule_sensorDeviceId_idx" ON "AutomationRule"("sensorDeviceId");
CREATE INDEX IF NOT EXISTS "AutomationRule_targetDeviceId_idx" ON "AutomationRule"("targetDeviceId");
CREATE INDEX IF NOT EXISTS "EventLog_gatewayId_idx" ON "EventLog"("gatewayId");
CREATE INDEX IF NOT EXISTS "EventLog_eventType_idx" ON "EventLog"("eventType");
CREATE INDEX IF NOT EXISTS "EventLog_occurredAt_idx" ON "EventLog"("occurredAt");
CREATE INDEX IF NOT EXISTS "TelemetryLog_deviceId_idx" ON "TelemetryLog"("deviceId");
CREATE INDEX IF NOT EXISTS "TelemetryLog_gatewayId_idx" ON "TelemetryLog"("gatewayId");
CREATE INDEX IF NOT EXISTS "TelemetryLog_recordedAt_idx" ON "TelemetryLog"("recordedAt");

-- 4. Add Foreign Keys (Safe constraint addition)
DO $$ BEGIN
    ALTER TABLE "Gateway" ADD CONSTRAINT "Gateway_userId_fkey" FOREIGN KEY ("userId") REFERENCES "User"("id") ON DELETE SET NULL ON UPDATE CASCADE;
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    ALTER TABLE "Device" ADD CONSTRAINT "Device_gatewayId_fkey" FOREIGN KEY ("gatewayId") REFERENCES "Gateway"("id") ON DELETE CASCADE ON UPDATE CASCADE;
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    ALTER TABLE "Schedule" ADD CONSTRAINT "Schedule_gatewayId_fkey" FOREIGN KEY ("gatewayId") REFERENCES "Gateway"("id") ON DELETE CASCADE ON UPDATE CASCADE;
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    ALTER TABLE "AutomationRule" ADD CONSTRAINT "AutomationRule_gatewayId_fkey" FOREIGN KEY ("gatewayId") REFERENCES "Gateway"("id") ON DELETE CASCADE ON UPDATE CASCADE;
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    ALTER TABLE "EventLog" ADD CONSTRAINT "EventLog_gatewayId_fkey" FOREIGN KEY ("gatewayId") REFERENCES "Gateway"("id") ON DELETE RESTRICT ON UPDATE CASCADE;
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

DO $$ BEGIN
    ALTER TABLE "TelemetryLog" ADD CONSTRAINT "TelemetryLog_gatewayId_fkey" FOREIGN KEY ("gatewayId") REFERENCES "Gateway"("id") ON DELETE RESTRICT ON UPDATE CASCADE;
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;

-- 5. Seed default Gateway for immediate pairing
INSERT INTO "Gateway" ("id", "name", "status", "firmwareVersion", "timezone", "claimCode")
VALUES ('gw_smartclock_01', 'Smart Clock Gateway 01', 'online', '1.6.0', 'ICT-7', 'CLAIM123')
ON CONFLICT ("id") DO NOTHING;
