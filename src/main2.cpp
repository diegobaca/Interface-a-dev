#include <WiFiS3.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// WiFi credentials
const char* ssid = "Brainiac";
const char* password = "8128281282";

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiServer server(5001); // Web server on port 5001

// Arduino pins connected to Interface A outputs
const int outputPins[6] = {2, 3, 4, 5, 6, 7}; // Digital pins for outputs 0–5
bool outputStates[6] = {false, false, false, false, false, false}; // State of outputs 0–5

// Track last pressed direction for each bridge (A, B, C)
char lastDirection[3] = {'\0', '\0', '\0'}; // Tracks 'L' or 'R' for bridges A, B, C

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Initialize the OLED display
  if (!display.begin(0x3C)) { // Use the I2C address directly
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting...");
  display.display();

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Display "INTERFACE-A" and IP with port 5001 on the OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Control Lab IO"); // First line
  display.setCursor(0, 10);
  display.println("Interface-A");   // Second line
  display.setCursor(0, 20);
  display.print(WiFi.localIP());    // Third line
  display.println(":5001");         // Add the port to the IP
  display.display();

  // Initialize pins for outputs 0–5
  for (int i = 0; i < 6; i++) {
    pinMode(outputPins[i], OUTPUT);
    digitalWrite(outputPins[i], LOW); // Ensure all outputs are OFF initially
  }

  server.begin(); // Start the server
  Serial.println("Server started!");
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("Client connected.");

  // Wait for client data
  while (client.connected() && !client.available()) {
    delay(1);
  }

  // Read client request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush(); // Clear the buffer

  // Handle toggle requests for outputs 0–5
  for (int i = 0; i < 6; i++) {
    if (request.indexOf("GET /toggle" + String(i)) >= 0) {
      Serial.println("Toggling output " + String(i));
      handleIndividualToggle(i); // Toggle the specified output
    }
  }

  // Handle bridge commands for A, B, and C
  handleBridge(request, 'A', 0, 1); // Bridge A: Outputs 0 and 1
  handleBridge(request, 'B', 2, 3); // Bridge B: Outputs 2 and 3
  handleBridge(request, 'C', 4, 5); // Bridge C: Outputs 4 and 5

  // Serve JSON response for state updates
  if (request.indexOf("GET /state") >= 0) {
    String jsonResponse = getOutputStatesJSON();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println(jsonResponse);
  }
  // Serve HTML for the main page
  else if (request.indexOf("GET /") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    serveHTML(client); // Use a helper function to keep the code clean
  }

  delay(1); // Ensure the response is sent
  client.stop(); // Close the connection
  Serial.println("Client disconnected.");
}

void serveHTML(WiFiClient &client) {
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>LEGO Interface A Outputs</title>");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; }");
  client.println("h1, h2, h3 { margin: 10px 0; }");
  client.println("button { padding: 15px 20px; font-size: 16px; margin: 10px; border: 5px solid black; border-radius: 5px; cursor: pointer; width: 80%; max-width: 300px; }");
  client.println(".active { border-color: green; }");
  client.println("@media (max-width: 600px) { button { font-size: 14px; padding: 10px 15px; } h1, h2, h3 { font-size: 18px; } }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<h1>LEGO Interface A Outputs</h1>");
  client.println("<h2>Control Outputs 0-5</h2>");
  for (int i = 0; i < 6; i++) {
    client.println("<button id=\"output" + String(i) + "\" onclick=\"toggleOutput(" + String(i) + ")\">Toggle Output " + String(i) + "</button><br>");
  }
  client.println("<h2>Control Bridges A-C</h2>");
  client.println("<h3>Bridge A</h3>");
  client.println("<button id=\"bridgeA_L\" onclick=\"bridge('A', 'L')\">L</button>");
  client.println("<button id=\"bridgeA_R\" onclick=\"bridge('A', 'R')\">R</button><br>");
  client.println("<h3>Bridge B</h3>");
  client.println("<button id=\"bridgeB_L\" onclick=\"bridge('B', 'L')\">L</button>");
  client.println("<button id=\"bridgeB_R\" onclick=\"bridge('B', 'R')\">R</button><br>");
  client.println("<h3>Bridge C</h3>");
  client.println("<button id=\"bridgeC_L\" onclick=\"bridge('C', 'L')\">L</button>");
  client.println("<button id=\"bridgeC_R\" onclick=\"bridge('C', 'R')\">R</button><br>");
  client.println("<script>");
  client.println("  function fetchStatesAndUpdate() {");
  client.println("    fetch('/state') // Fetch state from the server");
  client.println("      .then((response) => response.json())");
  client.println("      .then((data) => updateButtonStyles(data))");
  client.println("      .catch((error) => console.error('Error fetching states:', error));");
  client.println("  }");
  client.println("  function toggleOutput(output) {");
  client.println("    const button = document.getElementById(`output${output}`);");
  client.println("    button.classList.toggle('active'); // Immediate feedback on button press");
  client.println("    fetch(`/toggle${output}`) // Send toggle request to the server");
  client.println("      .catch((error) => console.error('Error toggling output:', error));");
  client.println("  }");
  client.println("  function bridge(type, direction) {");
  client.println("    const leftButton = document.getElementById(`bridge${type}_L`);");
  client.println("    const rightButton = document.getElementById(`bridge${type}_R`);");
  client.println("    if (direction === 'L') {");
  client.println("      leftButton.classList.add('active');");
  client.println("      rightButton.classList.remove('active');");
  client.println("    } else if (direction === 'R') {");
  client.println("      rightButton.classList.add('active');");
  client.println("      leftButton.classList.remove('active');");
  client.println("    }");
  client.println("    fetch(`/bridge${type}/${direction}`) // Send bridge command to the server");
  client.println("      .catch((error) => console.error('Error setting bridge direction:', error));");
  client.println("  }");
  client.println("  function updateButtonStyles(data) {");
  client.println("    // Update outputs");
  client.println("    for (let i = 0; i < 6; i++) {");
  client.println("      const button = document.getElementById(`output${i}`);");
  client.println("      if (data.o[i] === 1) { // 1 means ON");
  client.println("        button.classList.add('active');");
  client.println("      } else {");
  client.println("        button.classList.remove('active');");
  client.println("      }");
  client.println("    }");
  client.println("    // Update bridges");
  client.println("    ['A', 'B', 'C'].forEach((bridge, index) => {");
  client.println("      const leftButton = document.getElementById(`bridge${bridge}_L`);");
  client.println("      const rightButton = document.getElementById(`bridge${bridge}_R`);");
  client.println("      if (data.b[index] === 'L') {");
  client.println("        leftButton.classList.add('active'); rightButton.classList.remove('active');");
  client.println("      } else if (data.b[index] === 'R') {");
  client.println("        rightButton.classList.add('active'); leftButton.classList.remove('active');");
  client.println("      } else {");
  client.println("        leftButton.classList.remove('active'); rightButton.classList.remove('active');");
  client.println("      }");
  client.println("    });");
  client.println("  }");
  client.println("  // Fetch the initial state and update the UI");
  client.println("  fetchStatesAndUpdate();");
  client.println("  // Periodically fetch state updates every 0.5 seconds");
  client.println("  setInterval(fetchStatesAndUpdate, 500);");
  client.println("</script>");
  client.println("</body>");
  client.println("</html>");
}

String getOutputStatesJSON() {
  String json = "{ \"o\": [";
  for (int i = 0; i < 6; i++) {
    json += outputStates[i] ? "1" : "0"; // Convert boolean to integer
    if (i < 5) json += ","; // Add comma between items
  }
  json += "], \"b\": [";
  for (int i = 0; i < 3; i++) {
    if (lastDirection[i] == 'L') {
      json += "\"L\"";
    } else if (lastDirection[i] == 'R') {
      json += "\"R\"";
    } else {
      json += "\"N\""; // Represent null/default state as "N"
    }
    if (i < 2) json += ","; // Add comma between items
  }
  json += "] }";
  return json;
}

// Handle individual toggles
void handleIndividualToggle(int pin) {
  resetBridgeForOutput(pin); // Reset any active bridge for this output
  outputStates[pin] = !outputStates[pin]; // Toggle the output state
  digitalWrite(outputPins[pin], outputStates[pin] ? HIGH : LOW);
  Serial.println("Output " + String(pin) + (outputStates[pin] ? " ON" : " OFF"));
}

// Function to handle bridge commands
void handleBridge(String request, char bridge, int pin0, int pin1) {
  String leftCmd = "GET /bridge";
  leftCmd += bridge;
  leftCmd += "/L";
  String rightCmd = "GET /bridge";
  rightCmd += bridge;
  rightCmd += "/R";

  int bridgeIndex = bridge - 'A';

  if (request.indexOf(leftCmd) >= 0) {
    // If already set to 'L', toggle to null
    if (lastDirection[bridgeIndex] == 'L') {
      resetOutputsInBridge(pin0, pin1); // Turn off both outputs
      lastDirection[bridgeIndex] = '\0'; // Set state to null
      Serial.println("Bridge " + String(bridge) + " set to null.");
    } else {
      // Otherwise, toggle to 'L'
      resetOutputsInBridge(pin0, pin1); // Ensure outputs are off before toggling
      outputStates[pin1] = true;  // Turn ON Output 1
      outputStates[pin0] = false; // Ensure Output 0 is OFF
      digitalWrite(outputPins[pin1], HIGH);
      digitalWrite(outputPins[pin0], LOW);
      lastDirection[bridgeIndex] = 'L'; // Update state to 'L'
      Serial.println("Bridge " + String(bridge) + " Left toggled.");
    }
  } else if (request.indexOf(rightCmd) >= 0) {
    // If already set to 'R', toggle to null
    if (lastDirection[bridgeIndex] == 'R') {
      resetOutputsInBridge(pin0, pin1); // Turn off both outputs
      lastDirection[bridgeIndex] = '\0'; // Set state to null
      Serial.println("Bridge " + String(bridge) + " set to null.");
    } else {
      // Otherwise, toggle to 'R'
      resetOutputsInBridge(pin0, pin1); // Ensure outputs are off before toggling
      outputStates[pin0] = true;  // Turn ON Output 0
      outputStates[pin1] = false; // Ensure Output 1 is OFF
      digitalWrite(outputPins[pin0], HIGH);
      digitalWrite(outputPins[pin1], LOW);
      lastDirection[bridgeIndex] = 'R'; // Update state to 'R'
      Serial.println("Bridge " + String(bridge) + " Right toggled.");
    }
  }
}

void resetOutputsInBridge(int pin0, int pin1) {
  digitalWrite(outputPins[pin0], LOW);
  digitalWrite(outputPins[pin1], LOW);
  outputStates[pin0] = false;
  outputStates[pin1] = false;
}

void resetBridgeForOutput(int pin) {
  if (pin == 0 || pin == 1) lastDirection[0] = '\0';
  if (pin == 2 || pin == 3) lastDirection[1] = '\0';
  if (pin == 4 || pin == 5) lastDirection[2] = '\0';
}