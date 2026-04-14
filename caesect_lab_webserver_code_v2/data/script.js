var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', () => {
    initWebSocket();
});

function initWebSocket() {
    console.log('Opening WebSocket...');
    websocket = new WebSocket(gateway);
    websocket.onopen = () => websocket.send("getValues");
    websocket.onclose = () => setTimeout(initWebSocket, 2000);
    websocket.onmessage = onMessage;
}

function onMessage(event) {
    // 1. Log the raw data to the browser console (Press F12 to see it)
    console.log("Data received from ESP32:", event.data);

    try {
        var myObj = JSON.parse(event.data);
        
        // 2. This loop looks at every key sent in the JSON (e.g., "tempValue1", "tempValue2")
        Object.keys(myObj).forEach(key => {
            // 3. It looks for an HTML element with that exact ID
            var element = document.getElementById(key);
            
            if (element) {
                // 4. Updates the text inside the <span> using innerHTML
                element.innerHTML = myObj[key];
                
                // Optional: Console log to confirm which IDs are being hit
                console.log("Success: Updated ID '" + key + "' with value: " + myObj[key]);
            } else {
                // If this triggers, your HTML ID doesn't match the JSON key
                console.warn("Warning: No HTML element found with ID: " + key);
            }
        });
    } catch (e) {
        console.error("JSON Parsing Error:", e);
    }
}
