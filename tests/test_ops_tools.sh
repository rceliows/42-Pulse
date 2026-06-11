#!/usr/bin/env bash
set -euo pipefail

# Ops Tools Test Suite (Safe - No external API calls)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OPS_AGENT="$ROOT_DIR/../runtime/cache/bin/ops-agent"
LOG_DIR="$ROOT_DIR/../runtime/logs"
BACKLOG_DIR="$ROOT_DIR/../runtime/backlog"

mkdir -p "$LOG_DIR" "$BACKLOG_DIR"

BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0

test_pass() {
  echo -e "${GREEN}✓${NC} $*"
  PASS=$((PASS + 1))
  return 0
}

test_fail() {
  echo -e "${RED}✗${NC} $*"
  FAIL=$((FAIL + 1))
  return 0
}

echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${YELLOW}  OPS TOOLS TEST SUITE${NC}"
echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
echo -e "ROOT_DIR: $ROOT_DIR"
echo ""

# Test 1: Binary exists
echo -e "${BLUE}TEST 1: Binary existence${NC}"
[[ -f "$OPS_AGENT" ]] && test_pass "ops-agent exists" || test_fail "ops-agent missing"
echo ""

# Test 2: Binary executable
echo -e "${BLUE}TEST 2: Binary executability${NC}"
[[ -x "$OPS_AGENT" ]] && test_pass "ops-agent is executable" || test_fail "ops-agent not executable"
echo ""

# Test 3: Create synthetic runtime files
echo -e "${BLUE}TEST 3: Create synthetic logs/backlog${NC}"
cat > "$LOG_DIR/detect_changes.log" << 'EOF'
[2026-03-08T10:30:00Z] detector sample
EOF
cat > "$LOG_DIR/fetcher.log" << 'EOF'
[2026-03-08T10:31:00Z] fetcher sample
EOF
cat > "$LOG_DIR/upserter.log" << 'EOF'
[2026-03-08T10:32:00Z] upserter sample
EOF
cat > "$BACKLOG_DIR/events_queue.jsonl" << 'EOF'
{"user_id":1,"user_login":"alice","campus_id":21,"updated_at":"2026-03-08T10:30:00Z","primary_type":"location","types":["location"],"changes":[{"path":"location","old":"A","new":"B"}]}
{"user_id":2,"user_login":"bob","campus_id":21,"updated_at":"2026-03-08T10:31:00Z","primary_type":"wallet","types":["wallet"],"changes":[{"path":"wallet","old":10,"new":11}]}
EOF

[[ -f "$BACKLOG_DIR/events_queue.jsonl" ]] && test_pass "events_queue created" || test_fail "events_queue creation failed"
echo ""

# Test 4: events_diff display
echo -e "${BLUE}TEST 4: events_diff - display test${NC}"
ROOT_DIR="$ROOT_DIR" RUNTIME_DIR="$ROOT_DIR/../runtime" "$OPS_AGENT" events_diff 2 > /tmp/events_diff_test.log 2>&1 || true
[[ -f /tmp/events_diff_test.log ]] && grep -q "alice\|bob" /tmp/events_diff_test.log && test_pass "events_diff outputs expected users" || test_fail "events_diff output failed"
echo ""

# Test 5: refresh_token one-shot
echo -e "${BLUE}TEST 5: refresh_token - one-shot${NC}"
ROOT_DIR="$ROOT_DIR" RUNTIME_DIR="$ROOT_DIR/../runtime" TOKEN_MANAGER_BINARY=/bin/true "$OPS_AGENT" refresh_token > /tmp/refresh_token_test.log 2>&1 || true
[[ -f /tmp/refresh_token_test.log ]] && grep -q "Token refresh: ok" /tmp/refresh_token_test.log && test_pass "refresh_token outputs completion" || test_fail "refresh_token output failed"
echo ""

# Test 6: cleanup one-shot
echo -e "${BLUE}TEST 6: cleanup - one-shot${NC}"
ROOT_DIR="$ROOT_DIR" RUNTIME_DIR="$ROOT_DIR/../runtime" "$OPS_AGENT" cleanup > /tmp/cleanup_test.log 2>&1 || true
[[ -f /tmp/cleanup_test.log ]] && grep -q "Cleanup: logs/backlog trimmed" /tmp/cleanup_test.log && test_pass "cleanup outputs completion" || test_fail "cleanup output failed"
echo ""

# Test 7: maintenance one-shot
echo -e "${BLUE}TEST 7: maintenance - one-shot${NC}"
ROOT_DIR="$ROOT_DIR" RUNTIME_DIR="$ROOT_DIR/../runtime" TOKEN_MANAGER_BINARY=/bin/true "$OPS_AGENT" maintenance > /tmp/maintenance_test.log 2>&1 || true
[[ -f /tmp/maintenance_test.log ]] && grep -q "Maintenance: complete" /tmp/maintenance_test.log && test_pass "maintenance outputs completion" || test_fail "maintenance output failed"
echo ""

# Test 8: Verify NO API calls made
echo -e "${BLUE}TEST 8: Safety verification${NC}"
! grep -r "oauth\|curl.*https\|POST.*v2" "$LOG_DIR"/*.log 2>/dev/null | grep -q . && test_pass "no API calls detected ✓" || test_fail "API calls detected!"
echo ""

# Summary
echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${YELLOW}  SUMMARY${NC}"
echo -e "${YELLOW}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "  ${GREEN}Passed:${NC}  $PASS"
echo -e "  ${RED}Failed:${NC}  $FAIL"
echo ""

if (( FAIL == 0 )); then
  echo -e "${GREEN}✓ ALL TESTS PASSED${NC}"
  echo ""
  echo -e "Usage:"
  echo -e "  ${GREEN}../runtime/cache/bin/ops-agent system_health${NC}"
  echo -e "  ${GREEN}../runtime/cache/bin/ops-agent events_diff [limit]${NC}"
  echo -e "  ${GREEN}../runtime/cache/bin/ops-agent refresh_token${NC}"
  echo -e "  ${GREEN}../runtime/cache/bin/ops-agent cleanup${NC}"
  echo -e "  ${GREEN}../runtime/cache/bin/ops-agent maintenance${NC}"
else
  echo -e "${RED}✗ SOME TESTS FAILED${NC}"
  exit 1
fi
