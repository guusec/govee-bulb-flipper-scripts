// Flipper Zero Govee Controller Script
// Creates UI with ON/OFF, hex input, and color preset buttons

let eventLoop = require("event_loop");
let gui = require("gui");
let submenuView = require("gui/submenu");
let textBoxView = require("gui/text_box");
let textInputView = require("gui/text_input");
let serial = require("serial");

// Initialize UART on usart port (pins 13 TX, 14 RX)
serial.setup("usart", 115200);

// Track serial state to prevent double cleanup
let serialState = { active: true };

// Create views
let views = {
    menu: submenuView.makeWith({
        header: "Govee Controller",
        items: [
            "ON",
            "OFF",
            "Enter Hex Color",
            "White (FFFFFF)",
            "Pink (FF15D8)",
            "Red (FF0000)",
            "Blue (0000FF)",
            "Green (00FF00)",
            "Yellow (FFFF00)",
            "Purple (8000FF)",
            "Orange (FF8000)",
            "Cyan (00FFFF)"
        ],
    }),
    output: textBoxView.make(),
    hexInput: textInputView.makeWith({
        header: "Enter Hex Color",
        minLength: 6,
        maxLength: 6,
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
        // ON button pressed
        sendCommandAndRead("ON");
    } else if (index === 1) {
        // OFF button pressed
        sendCommandAndRead("OFF");
    } else if (index === 2) {
        // Enter Hex Color - show text input
        gui.viewDispatcher.switchTo(views.hexInput);
    } else if (index === 3) {
        // White (FFFFFF)
        sendCommandAndRead("FFFFFF");
    } else if (index === 4) {
        // Pink (FF15D8)
        sendCommandAndRead("FF15D8");
    } else if (index === 5) {
        // Red (FF0000)
        sendCommandAndRead("FF0000");
    } else if (index === 6) {
        // Blue (0000FF)
        sendCommandAndRead("0000FF");
    } else if (index === 7) {
        // Green (00FF00)
        sendCommandAndRead("00FF00");
    } else if (index === 8) {
        // Yellow (FFFF00)
        sendCommandAndRead("FFFF00");
    } else if (index === 9) {
        // Purple (8000FF)
        sendCommandAndRead("8000FF");
    } else if (index === 10) {
        // Orange (FF8000)
        sendCommandAndRead("FF8000");
    } else if (index === 11) {
        // Cyan (00FFFF)
        sendCommandAndRead("00FFFF");
    }
}, gui, views);

// Handle hex input submission
eventLoop.subscribe(views.hexInput.input, function (_sub, text, gui, views) {
    // Validate hex input (should be 6 characters, all hex digits)
    if (text.length === 6) {
        let isValidHex = true;
        for (let i = 0; i < text.length; i++) {
            let char = text[i].toUpperCase();
            if (!((char >= '0' && char <= '9') || (char >= 'A' && char <= 'F'))) {
                isValidHex = false;
                break;
            }
        }

        if (isValidHex) {
            sendCommandAndRead(text.toUpperCase());
        } else {
            views.output.set("text", "Invalid hex color: " + text + "\nPlease use only 0-9 and A-F characters");
            gui.viewDispatcher.switchTo(views.output);
        }
    } else {
        views.output.set("text", "Invalid hex color length: " + text + "\nPlease enter exactly 6 characters");
        gui.viewDispatcher.switchTo(views.output);
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
