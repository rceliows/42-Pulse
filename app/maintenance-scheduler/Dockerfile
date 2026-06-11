FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        g++ \
        docker.io \
        docker-compose-v2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /srv/42_Network/repo

COPY app/maintenance-scheduler/src /tmp/maintenance-scheduler-src

RUN g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
      /tmp/maintenance-scheduler-src/maintenance_scheduler.cpp \
      -o /usr/local/bin/maintenance-scheduler-agent \
    && rm -rf /tmp/maintenance-scheduler-src

ENTRYPOINT ["/usr/local/bin/maintenance-scheduler-agent"]
