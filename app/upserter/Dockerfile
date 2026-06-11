FROM python:3.11-slim

LABEL maintainer="42_Network Ops" \
      description="Runtime for upserter agent"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      bash curl jq postgresql-client ca-certificates procps g++ && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY app /app/app
COPY third_party /app/third_party

RUN g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
      app/upserter/src/upserter.cpp \
      -o /usr/local/bin/upserter-agent

ENTRYPOINT ["/usr/local/bin/upserter-agent"]
