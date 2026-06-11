#!/usr/bin/env python3
"""File copy for UTM/virtio-9p shared filesystems.
  - cp fails because 9p rejects copy_file_range() with EAGAIN.
  - plain read() can stall when the 9p server is slow.
Strategy: try 9p with a short timeout; on failure fall back to a local cache
stored on the VM's native filesystem so fclean/down always work."""
import os, sys, time, errno, signal, shutil

if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <src> <dst>", file=sys.stderr)
    sys.exit(1)

src, dst      = sys.argv[1], sys.argv[2]
CACHE_DIR     = '/root/.cache/transcendence'
cache_path    = os.path.join(CACHE_DIR, os.path.basename(src))
ATTEMPT_SECS  = 8
MAX_ATTEMPTS  = 3

class _Timeout(Exception):
    pass

def _alarm(signum, frame):
    raise _Timeout()

def try_from_9p():
    for attempt in range(MAX_ATTEMPTS):
        signal.signal(signal.SIGALRM, _alarm)
        signal.alarm(ATTEMPT_SECS)
        try:
            fd = os.open(src, os.O_RDONLY)
            chunks = []
            try:
                while True:
                    try:
                        chunk = os.read(fd, 65536)
                        if not chunk:
                            break
                        chunks.append(chunk)
                    except OSError as e:
                        if e.errno == errno.EAGAIN:
                            signal.alarm(ATTEMPT_SECS)
                            time.sleep(0.1)
                            continue
                        raise
            finally:
                os.close(fd)
            signal.alarm(0)
            return b''.join(chunks)
        except (_Timeout, OSError):
            signal.alarm(0)
            if attempt < MAX_ATTEMPTS - 1:
                time.sleep(0.5)
    return None

data = try_from_9p()
if data is not None:
    with open(dst, 'wb') as f:
        f.write(data)
    os.makedirs(CACHE_DIR, exist_ok=True)
    with open(cache_path, 'wb') as f:
        f.write(data)
    sys.exit(0)

if os.path.exists(cache_path):
    print(f"utm_copy: 9p stalled, using cached {os.path.basename(src)}", file=sys.stderr)
    shutil.copy2(cache_path, dst)
    sys.exit(0)

print(f"utm_copy: 9p failed and no cache at {cache_path}", file=sys.stderr)
print(f"utm_copy: seed it once with:  make cache-compose", file=sys.stderr)
sys.exit(1)
