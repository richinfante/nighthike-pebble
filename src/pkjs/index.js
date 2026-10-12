var locationPending = false;

function requestLocation() {
  if (locationPending) {
    return;
  }
  if (!navigator.geolocation) {
    Pebble.sendAppMessage({ LocationStatus: 2 });
    return;
  }
  locationPending = true;
  navigator.geolocation.getCurrentPosition(function(position) {
    locationPending = false;
    var latitude = position.coords.latitude;
    var longitude = position.coords.longitude;
    var altitude = position.coords.altitude;

    // demo coords
    // latitude = 40.7128
    // longitude = -74.0060;
    // altitude = 10.0;

    var altitudeValid = typeof altitude === 'number' && isFinite(altitude) &&
      altitude >= -2147483648 && altitude <= 2147483647;
    if (!isFinite(latitude) || !isFinite(longitude) ||
        Math.abs(latitude) > 90 || Math.abs(longitude) > 180) {
      Pebble.sendAppMessage({ LocationStatus: 2 });
      return;
    }
    Pebble.sendAppMessage({
      Latitude: Math.round(latitude * 1000000),
      Longitude: Math.round(longitude * 1000000),
      Altitude: altitudeValid ? Math.round(altitude) : 0,
      AltitudeValid: altitudeValid ? 1 : 0,
      Accuracy: typeof position.coords.accuracy === 'number' &&
        isFinite(position.coords.accuracy) && position.coords.accuracy >= 0 ?
        Math.min(2147483647, Math.ceil(position.coords.accuracy)) : -1,
      LocationStatus: 0
    });
  }, function(error) {
    locationPending = false;
    Pebble.sendAppMessage({ LocationStatus: error.code || 2 });
  }, {
    enableHighAccuracy: true,
    maximumAge: 30000,
    timeout: 20000
  });
}

Pebble.addEventListener('ready', function() {
  requestLocation();
  setInterval(requestLocation, 60000);
});

Pebble.addEventListener('appmessage', function(event) {
  if (event.payload.RequestLocation) {
    requestLocation();
  }
});