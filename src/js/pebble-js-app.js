Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
});

const COMMAND_ADD_TO_TIMELINE  =       0;
const COMMAND_REMOVE_FROM_TIMELINE =   1;

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage', function(e) {
    console.log("Received message: " + JSON.stringify(e.payload));
    
    if (e.payload.KEY_COMMAND == COMMAND_ADD_TO_TIMELINE) {
        var date = new Date();
        date.setTime(date.getTime() + e.payload.KEY_TIMELINE_TIME * 1000);
        
        var pin = {
            "id": "pin-" + e.payload.KEY_TIMELINE_ID,
            "time": date.toISOString(),
            "layout": {
                "type": "genericPin",
                "title": e.payload.KEY_TIMELINE_TITLE,
                "tinyIcon": "system://images/NOTIFICATION_GENERIC"
            }
        };
        
        console.log('Inserting pin: ' + JSON.stringify(pin));
        
        insertUserPin(pin, function(responseText) { 
            console.log('Add result: ' + responseText);
        });
    } else if (e.payload.KEY_COMMAND == COMMAND_REMOVE_FROM_TIMELINE) {
        var pin = {
            "id": "pin-" + e.payload.KEY_TIMELINE_ID,
        };
        
        console.log('Removing pin: ' + JSON.stringify(pin));
        
        deleteUserPin(pin, function(responseText) {
            console.log('Remove result: ' + responseText);
        });
    }
});

/******************************* timeline lib *********************************/

// The timeline public URL root
var API_URL_ROOT = 'https://timeline-api.getpebble.com/';

/**
 * Send a request to the Pebble public web timeline API.
 * @param pin The JSON pin to insert. Must contain 'id' field.
 * @param type The type of request, either PUT or DELETE.
 * @param callback The callback to receive the responseText after the request has completed.
 */
function timelineRequest(pin, type, callback) {
  // User or shared?
  var url = API_URL_ROOT + 'v1/user/pins/' + pin.id;

  // Create XHR
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    console.log('timeline: response received: ' + this.responseText);
    callback(this.responseText);
  };
  xhr.open(type, url);

  // Get token
  Pebble.getTimelineToken(function(token) {
    // Add headers
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', '' + token);

    // Send
    xhr.send(JSON.stringify(pin));
    console.log('timeline: request sent.');
  }, function(error) { console.log('timeline: error getting timeline token: ' + error); });
}

/**
 * Insert a pin into the timeline for this user.
 * @param pin The JSON pin to insert.
 * @param callback The callback to receive the responseText after the request has completed.
 */
function insertUserPin(pin, callback) {
  timelineRequest(pin, 'PUT', callback);
}

/**
 * Delete a pin from the timeline for this user.
 * @param pin The JSON pin to delete.
 * @param callback The callback to receive the responseText after the request has completed.
 */
function deleteUserPin(pin, callback) {
  timelineRequest(pin, 'DELETE', callback);
}

/***************************** end timeline lib *******************************/
