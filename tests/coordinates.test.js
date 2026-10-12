const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const test = require('node:test');

test('C formatter rounds microdegrees to five decimals with hemisphere suffixes', () => {
  const source = fs.readFileSync(path.join(__dirname, '../src/c/nighthike.c'), 'utf8');
  const start = source.indexOf('static void prv_format_coordinate(');
  const end = source.indexOf('\nstatic void prv_inbox_received(', start);
  assert.ok(start >= 0 && end > start);
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'nighthike-test-'));
  const executable = path.join(directory, 'coordinates');
  const harness = `
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
${source.slice(start, end)}
int main(void) {
  char buffer[20];
  prv_format_coordinate(buffer, sizeof(buffer), 37774929, 'N', 'S');
  assert(strcmp(buffer, "37.77493N") == 0);
  prv_format_coordinate(buffer, sizeof(buffer), -122419416, 'E', 'W');
  assert(strcmp(buffer, "122.41942W") == 0);
  prv_format_coordinate(buffer, sizeof(buffer), 12999995, 'N', 'S');
  assert(strcmp(buffer, "13.00000N") == 0);
  prv_format_coordinate(buffer, sizeof(buffer), -5, 'N', 'S');
  assert(strcmp(buffer, "0.00001S") == 0);
  prv_format_coordinate(buffer, sizeof(buffer), 0, 'E', 'W');
  assert(strcmp(buffer, "0.00000E") == 0);
  prv_format_coordinate(buffer, sizeof(buffer), 180000000, 'E', 'W');
  assert(strcmp(buffer, "180.00000E") == 0);
  return 0;
}
`;
  try {
    const compiled = spawnSync('cc', ['-x', 'c', '-', '-o', executable], {
      input: harness, encoding: 'utf8'
    });
    assert.equal(compiled.status, 0, compiled.stderr || String(compiled.error));
    const result = spawnSync(executable, [], { encoding: 'utf8' });
    assert.equal(result.status, 0, result.stderr);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});