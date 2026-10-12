const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const test = require('node:test');

function createPhone() {
  const handlers = {};
  const messages = [];
  const requests = [];
  const timers = [];
  const context = {
    Pebble: {
      addEventListener: (name, handler) => { handlers[name] = handler; },
      sendAppMessage: message => { messages.push(message); }
    },
    navigator: {
      geolocation: {
        getCurrentPosition: (success, failure, options) => {
          requests.push({ success, failure, options });
        }
      }
    },
    setInterval: (callback, delay) => { timers.push({ callback, delay }); }
  };
  const source = fs.readFileSync(path.join(__dirname, '../src/pkjs/index.js'), 'utf8');
  vm.runInNewContext(source, context);
  return { context, handlers, messages, requests, timers };
}

test('requests GPS on ready and sends signed microdegrees', () => {
  const phone = createPhone();
  phone.handlers.ready();
  assert.equal(phone.requests.length, 1);
  assert.equal(phone.requests[0].options.enableHighAccuracy, true);
  assert.equal(phone.requests[0].options.timeout, 20000);
  assert.equal(phone.timers[0].delay, 60000);
  phone.requests[0].success({ coords: { latitude: -33.8688, longitude: 151.2093 } });
  assert.deepEqual(JSON.parse(JSON.stringify(phone.messages[0])), {
    Latitude: -33868800, Longitude: 151209300, Altitude: 0,
    AltitudeValid: 0, Accuracy: -1, LocationStatus: 0
  });
});

test('refreshes periodically and on request without overlapping requests', () => {
  const phone = createPhone();
  phone.handlers.ready();
  phone.handlers.appmessage({ payload: { RequestLocation: 1 } });
  assert.equal(phone.requests.length, 1);
  phone.requests[0].success({ coords: { latitude: 0, longitude: 0 } });
  phone.timers[0].callback();
  assert.equal(phone.requests.length, 2);
  phone.requests[1].failure({ code: 3 });
  phone.handlers.appmessage({ payload: { RequestLocation: 1 } });
  assert.equal(phone.requests.length, 3);
  phone.requests[2].success({ coords: { latitude: 37.774929, longitude: -122.419416 } });
  assert.equal(phone.messages[2].Latitude, 37774929);
  assert.equal(phone.messages[2].Longitude, -122419416);
});

test('reports permission denial and allows retry', () => {
  const phone = createPhone();
  phone.handlers.ready();
  phone.requests[0].failure({ code: 1 });
  assert.equal(phone.messages[0].LocationStatus, 1);
  phone.timers[0].callback();
  assert.equal(phone.requests.length, 2);
});

test('rejects invalid coordinates', () => {
  for (const coords of [
    { latitude: 91, longitude: 0 },
    { latitude: 0, longitude: -181 },
    { latitude: NaN, longitude: 0 },
    { latitude: 0, longitude: Infinity }
  ]) {
    const phone = createPhone();
    phone.handlers.ready();
    phone.requests[0].success({ coords });
    assert.equal(phone.messages[0].LocationStatus, 2);
    assert.equal(phone.messages[0].Latitude, undefined);
  }
});

test('reports unavailable geolocation', () => {
  const phone = createPhone();
  phone.context.navigator.geolocation = null;
  phone.handlers.ready();
  assert.equal(phone.messages[0].LocationStatus, 2);
});

test('sends conservative accuracy in whole metres and handles missing accuracy', () => {
  for (const [accuracy, expected] of [
    [4.2, 5], [0, 0], [null, -1], [undefined, -1],
    [NaN, -1], [Infinity, -1], [-1, -1], [3e9, 2147483647]
  ]) {
    const phone = createPhone();
    phone.handlers.ready();
    phone.requests[0].success({ coords: { latitude: 0, longitude: 0, accuracy } });
    assert.equal(phone.messages[0].Accuracy, expected);
  }
});

test('sends signed altitude and distinguishes missing altitude from sea level', () => {
  for (const [altitude, expected, valid] of [
    [1234.6, 1235, 1], [-430.2, -430, 1], [0, 0, 1],
    [null, 0, 0], [undefined, 0, 0], [NaN, 0, 0],
    [Infinity, 0, 0], [3e9, 0, 0], [-3e9, 0, 0]
  ]) {
    const phone = createPhone();
    phone.handlers.ready();
    phone.requests[0].success({ coords: { latitude: 0, longitude: 0, altitude } });
    assert.equal(phone.messages[0].Altitude, expected);
    assert.equal(phone.messages[0].AltitudeValid, valid);
  }
});