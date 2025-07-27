// Flipper Zero Color Controller Script
// Creates UI with WHITE and PINK buttons that send commands via UART

let eventLoop = require("event_loop");
let gui = require("gui");
let submenuView = require("gui/submenu");
let textBoxView = require("gui/text_box");
let serial = require("serial");

// Initialize UART on usart port (pins 13 TX, 14 RX)
serial.setup("usart", 115200);

// Track serial state to prevent double cleanup
let serialState = { active: true };

// Create views
let views = {
    menu: submenuView.makeWith({
        header: "Color Controller",
        items: [
            "WHITE",
            "PINK"
        ],
    }),
    output: textBoxView.makeWith({
        text: "Waiting for response...",
    })
};

// Function to send command and read response
function sendCommandAndRead(command) {
    // Send the command string over UART
    serial.write(command + "\n");

    // Read response from UART
    let response = "";
    let maxAttempts = 200; // Maximum read attempts (about 2 seconds with 10ms delays)
    let attempts = 0;

    // Try to read response data
    while (attempts < maxAttempts) {
        let rxData = serial.readBytes(1, 10); // Read 1 byte with 10ms timeout

        if (rxData !== undefined) {
            let dataView = Uint8Array(rxData);
            let char = chr(dataView[0]);
            response += char;

            // Check if we got a complete line (newline character)
            if (dataView[0] === 10 || dataView[0] === 13) { // \n or \r
                break;
            }

            // Prevent extremely long responses
            if (response.length > 200) {
                break;
            }
        }

        attempts++;
        delay(10); // Small delay between attempts
    }

    // If no response received, show a default message
    if (response.length === 0) {
        response = "No response received from peripheral device";
    }

    // Update the text box with the response
    views.output.set("text", "Command sent: " + command + "\n\nResponse:\n" + response);
    gui.viewDispatcher.switchTo(views.output);
}

// Handle menu selection
eventLoop.subscribe(views.menu.chosen, function (_sub, index, gui, views) {
    if (index === 0) {
        // WHITE button pressed
        sendCommandAndRead("WHITE");
    } else if (index === 1) {
        // PINK button pressed  
        sendCommandAndRead("PINK");
    }
}, gui, views);

// Handle back button navigation
eventLoop.subscribe(gui.viewDispatcher.navigation, function (_sub, _, gui, views, eventLoop, serialState) {
    if (gui.viewDispatcher.currentView === views.menu) {
        // If we're on the main menu and back is pressed, exit gracefully
        if (serialState.active) {
            serial.end(); // Clean up UART connection
            serialState.active = false;
        }
        eventLoop.stop();
        return;
    }
    // Otherwise go back to the main menu
    gui.viewDispatcher.switchTo(views.menu);
}, gui, views, eventLoop, serialState);

// Start the application
gui.viewDispatcher.switchTo(views.menu);
eventLoop.run();

// Serial cleanup is handled in the navigation handler