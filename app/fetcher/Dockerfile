FROM python:3.11-slim

LABEL maintainer="42_Network Ops" \
      description="Runtime for shared fetcher agent (internal + external queues)"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      bash curl jq postgresql-client ca-certificates procps g++ && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY app /app/app
COPY tools /app/tools
COPY third_party /app/third_party

RUN g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
      app/fetcher/src/fetcher.cpp \
      -o /usr/local/bin/fetcher-agent

RUN g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
      tools/token-manager/src/token_manager.cpp \
      -o /usr/local/bin/token-manager-agent

ENTRYPOINT ["/usr/local/bin/fetcher-agent"]
