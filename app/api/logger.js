import pino from "pino";

const logger = pino({
  level: process.env.LOG_LEVEL || "info",
  serializers: {
    err: pino.stdSerializers.err,
  },
});

export default logger;
