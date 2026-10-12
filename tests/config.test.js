const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

test('Clay preferences default to metric units and a red backlight', () => {
  const config = JSON.parse(fs.readFileSync(
    path.join(__dirname, '../src/pkjs/config.json'), 'utf8'));
  const options = config.flatMap(item => item.items || []);
  const units = options.find(item => item.messageKey === 'Units');
  const backlight = options.find(item => item.messageKey === 'BacklightColor');

  assert.equal(units.defaultValue, 'metric');
  assert.deepEqual(units.options.map(option => option.value), ['metric', 'imperial']);
  assert.equal(backlight.defaultValue, 'red');
  assert.deepEqual(backlight.options.map(option => option.value), [
    'red', 'orange', 'yellow', 'green', 'blue', 'white', 'system'
  ]);
});