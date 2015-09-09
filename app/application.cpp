
#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <DeviceConfig.h>
#include <NetworkConfig.h>
#include <BrokerConfig.h>
#ifdef LCD_DISPLAY
#include <Libraries/LiquidCrystal/LiquidCrystal_I2C.h>

#define IP_STATE   0
#define IO_STATE   1

#endif

//#define WIFI_SSID "see_dum" // Put you SSID and Password here
//#define WIFI_PWD "0863219053"

//#define MQTT_USER	"3EB95B23"
//#define MQTT_PWD	"CE564FC1"

#define DEBUG_PRINT(x) {Serial.println(x);}
#define DEBUG_PRINTLN() {Serial.println();}

#ifdef LCD_DISPLAY
// SDA GPIO2, SCL GPIO0
#define I2C_LCD_ADDR 0x27
LiquidCrystal_I2C lcd(I2C_LCD_ADDR, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
#endif

bool mqttStatus;
Timer procTimer;
Timer statusTimer;
Timer restartTimer;

uint32_t getChipId(void);
void publishMessage();
void onMessageReceived(String topic, String message); // Forward declaration for our callback
// MQTT client
// For quickly check you can use: http://www.hivemq.com/demos/websocket-client/ (Connection= test.mosquitto.org:8080)
MqttClient *mqtt;
HttpServer *server;
FTPServer *ftp;
BssList networks;
bool state = true;

String deviceId = String(getChipId());
int totalActiveSockets = 0;
#define INT_PIN 0   // GPIO0
#ifndef LCD_DISPLAY
#define CONF_PIN 2
#endif
unsigned long pressed;
bool configStarted = false;
sc_status conf_status;

String getTopic  = "";
String setTopic = "";

#ifdef LCD_DISPLAY

void lcdSystemStarting()
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("System starting");
	lcd.setCursor(0,1);
	lcd.print("Please wait...");
}

void lcdMqttDisconnect()
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("MQTT Client:");
	lcd.setCursor(0,1);
	lcd.print("Reconnecting...");
}
void lcdMqttRestart()
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("MQTT Client:");
	lcd.setCursor(0,1);
	lcd.print("Connected...");

}
void lcdStartServers()
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("AP Address:");
	lcd.setCursor(0,1);
	lcd.print("  "+WifiAccessPoint.getIP().toString()+"  ");
}

void lcdConnectOk()
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("IP Address:");
	lcd.setCursor(0,1);
	lcd.print("  "+WifiStation.getIP().toString()+"  ");
}
#endif

#ifndef LCD_DISPLAY
void blink()
{
	digitalWrite(CONF_PIN, state);
	state = !state;
}
#endif

uint32_t getChipId(void)
{
    return system_get_chip_id();
}

void restart()
{
    System.restart();
}

void allOutputOff()
{
	digitalWrite(12, LOW);
	digitalWrite(13, LOW);
	digitalWrite(14, LOW);
	digitalWrite(4, LOW);
#ifndef LCD_DISPLAY
	digitalWrite(CONF_PIN, HIGH);
#endif
}

void sendSocketClient(String msg)
{
	WebSocketsList &clients = server->getActiveWebSockets();
	for (int i = 0; i < clients.count(); i++)
		clients[i].sendString(msg);
}

void restartMqttConnection()
{
	DEBUG_PRINT("MQTT client reconnect");
	BrokerSettings.load();
	if (BrokerSettings.active == true){
		mqttStatus = true;
		sendSocketClient("{ \"Type\": 3, \"status\": true }");
		Serial.println("MQTT client reconnect");
		mqtt = new MqttClient(BrokerSettings.serverHost, BrokerSettings.serverPort, onMessageReceived);
		mqtt->connect(deviceId, BrokerSettings.user_name, BrokerSettings.password);
		mqtt->subscribe(getTopic);
		mqtt->subscribe(setTopic);
		procTimer.initializeMs(BrokerSettings.updateInterval * 1000, publishMessage).start();
#ifdef LCD_DISPLAY
		lcdConnectOk();
#endif
	}else{
		DEBUG_PRINT("MQTT client inactive...");
	}
}

// Publish our message
void publishMessage()
{
	if (mqtt->getConnectionState() != TcpClientState::eTCS_Connected){
		mqttStatus = false;
		sendSocketClient("{ \"Type\": 3, \"status\": false }");
		delete mqtt;
		procTimer.stop();
		Serial.println("MQTT client disconnected!");
		restartTimer.initializeMs(10000, restartMqttConnection).startOnce();
		Serial.println("MQTT client restarting...");
#ifdef LCD_DISPLAY
		lcdMqttDisconnect();
#endif
	}
}

// Callback for messages, arrived from MQTT server
void onMessageReceived(String topic, String message)
{
	DEBUG_PRINTLN();
	DEBUG_PRINT(topic);
	DEBUG_PRINT(" : "); // Prettify alignment for printing
	DEBUG_PRINT(message+"\r\n********************************\r\n");

	DynamicJsonBuffer jsonBuffer;

	BrokerSettings.load();
	int state = LOW;
	uint16_t gpio = -1;
	if (topic == setTopic){
		char* jsonString = new char[message.length()+1];
		message.toCharArray(jsonString, message.length()+1);
		JsonObject& root = jsonBuffer.parseObject(jsonString);
		Serial.println(root.toJsonString());
		String id = root["properties"][0]["id"].toString();
		String value = root["properties"][0]["value"].toString();

		DEBUG_PRINT("In Topic");
		DEBUG_PRINT("id : " + id);
		DEBUG_PRINT("value : " + value);
		String token;
		if (id == BrokerSettings.gpio12Id){
			state = value == "on" ? HIGH : LOW;
			token = BrokerSettings.gpio12Id;
			gpio = 12;
		}
		if (id == BrokerSettings.gpio13Id){
			state = value == "on" ? HIGH : LOW;
			token = BrokerSettings.gpio13Id;
			gpio = 13;
		}
		if (id == BrokerSettings.gpio14Id){
			state = value == "on" ? HIGH : LOW;
			token = BrokerSettings.gpio14Id;
			gpio = 14;
		}
		if (id == BrokerSettings.gpio4Id){
			state = value == "on" ? HIGH : LOW;
			token = BrokerSettings.gpio4Id;
			gpio = 4;
		}
		if (gpio != -1){
			digitalWrite(gpio, state);
			// websocket broadcast io status
			String st = digitalRead(gpio)?"1":"0";
			sendSocketClient("{ \"Type\": 2, \"gpio\": " + String(gpio) + ", \"status\": " + st  + " }");

			st = digitalRead(gpio)?"on":"off";
			String pubMsg = "{\"properties\":[{ \"token\": \"" + token + "\", \"status\": \"" + st + "\" }]}";
			mqtt->publish("device/"+BrokerSettings.user_name+"/"+BrokerSettings.token+"/status", pubMsg, true);
		}
		delete jsonString;
	}

	if (topic == getTopic){
		Serial.println("Out Topic");

		String status12 = digitalRead(12)?"on":"off";
		String status13 = digitalRead(13)?"on":"off";
		String status14 = digitalRead(14)?"on":"off";
		String status4  = digitalRead(4)?"on":"off";
		String pubMsg = "{\"properties\":[{ \"status\": \"" + status12  + "\", \"token\": \"" + BrokerSettings.gpio12Id + "\"" + " },{ \"status\": \"" + status13  + "\", \"token\": \"" + BrokerSettings.gpio13Id + "\"" + " },{ \"status\": \"" + status14 + "\", \"token\": \"" + BrokerSettings.gpio14Id + "\"" + " },{ \"status\": \"" + status4 + "\", \"token\": \"" + BrokerSettings.gpio4Id + "\"" + " }]}";
		//String pubMsg = "{\"properties\":[{ \"type\": 2, \"status\": \"" + status12  + "\", \"gpio\": 12 },{ \"type\": 2, \"status\": \"" + status13  + "\", \"gpio\": 13 },{ \"type\": 2, \"status\": \"" + status14 + "\", \"gpio\": 14 }]}";
		String pubTopic = "device/"+BrokerSettings.user_name+"/"+BrokerSettings.token+"/status";
		mqtt->publish(pubTopic, pubMsg, true);
		DEBUG_PRINT(pubTopic + ": " + pubMsg);

	}
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	vars["checked12"] = digitalRead(12)?"checked":"";
	vars["checked13"] = digitalRead(13)?"checked":"";
	vars["checked14"] = digitalRead(14)?"checked":"";
	vars["checked4"] = digitalRead(4)?"checked":"";
	vars["device_id"] = deviceId;
	vars["status"] = BrokerSettings.active ? mqttStatus ? "Connected" : "Disconnected" : "Disable";
	vars["label_class"] = BrokerSettings.active ? mqttStatus ? "label-success" : "label-danger" : "label-default";

	response.sendTemplate(tmpl); // this template object will be deleted automatically
}

void onNetworkConfig(HttpRequest &request, HttpResponse &response)
{
	DEBUG_PRINT("onNetworkConfig..");

	TemplateFileStream *tmpl = new TemplateFileStream("network.html");
	auto &vars = tmpl->variables();
	String option = "";
	for (int i = 0; i < networks.count(); i++)
	{
		if (networks[i].hidden) continue;
		if (networks[i].ssid == NetworkSettings.ssid){
			option += "<option selected>" + networks[i].ssid + "</option>";
		}else{
			option += "<option>" + networks[i].ssid + "</option>";
		}
	}
	//Serial.println("Option: "+ option);
	vars["option"] = option;

	if (request.getRequestMethod() == RequestMethod::POST)
	{
		NetworkSettings.ssid = request.getPostParameter("ssid");
		NetworkSettings.password = request.getPostParameter("password");
		NetworkSettings.dhcp = request.getPostParameter("dhcp") == "1";
		NetworkSettings.ip = request.getPostParameter("ip");
		NetworkSettings.netmask = request.getPostParameter("netmask");
		NetworkSettings.gateway = request.getPostParameter("gateway");
		debugf("Updating IP settings: %d", NetworkSettings.ip.isNull());
		NetworkSettings.save();

		vars["dhcpon"] = request.getPostParameter("dhcp") == "1" ? "checked='checked'" : "";
		vars["dhcpoff"] = request.getPostParameter("dhcp") == "0" ? "checked='checked'" : "";
		vars["password"] = request.getPostParameter("password");
		vars["ip"] = request.getPostParameter("ip");
		vars["netmask"] = request.getPostParameter("netmask");
		vars["gateway"] = request.getPostParameter("gateway");

	}else{
		bool dhcp = WifiStation.isEnabledDHCP();
		vars["dhcpon"] = dhcp ? "checked='checked'" : "";
		vars["dhcpoff"] = !dhcp ? "checked='checked'" : "";
		vars["password"] = NetworkSettings.password;
		if (!WifiStation.getIP().isNull())
		{
			vars["ip"] = WifiStation.getIP().toString();
			vars["netmask"] = WifiStation.getNetworkMask().toString();
			vars["gateway"] = WifiStation.getNetworkGateway().toString();
		}
		else
		{
			vars["ip"] = "192.168.1.200";
			vars["netmask"] = "255.255.255.0";
			vars["gateway"] = "192.168.1.1";
		}
	}
	response.sendTemplate(tmpl); // will be automatically deleted
}

void onBrokerConfig(HttpRequest &request, HttpResponse &response)
{
	DEBUG_PRINT("onBrokerConfig..");
	TemplateFileStream *tmpl = new TemplateFileStream("config.html");
	auto &vars = tmpl->variables();

	if (request.getRequestMethod() == RequestMethod::POST)
	{
		BrokerSettings.active = request.getPostParameter("mqtt") == "1";
		BrokerSettings.user_name = request.getPostParameter("user_name");
		BrokerSettings.password = request.getPostParameter("password");
		BrokerSettings.serverHost = request.getPostParameter("broker");
		BrokerSettings.serverPort = request.getPostParameter("port").toInt();
		BrokerSettings.updateInterval = request.getPostParameter("interval").toInt();
		BrokerSettings.token = request.getPostParameter("token");
		BrokerSettings.gpio12Id = request.getPostParameter("gpio12");
		BrokerSettings.gpio13Id = request.getPostParameter("gpio13");
		BrokerSettings.gpio14Id = request.getPostParameter("gpio14");
		BrokerSettings.gpio4Id = request.getPostParameter("gpio4");

		BrokerSettings.save();
	}
	BrokerSettings.load();
	vars["mqtton"] = BrokerSettings.active ? "checked='checked'" : "";
	vars["mqttoff"] = !BrokerSettings.active ? "checked='checked'" : "";
	vars["user_name"] = BrokerSettings.user_name;
	vars["password"] = BrokerSettings.password;
	vars["broker"] = BrokerSettings.serverHost;
	vars["port"] = BrokerSettings.serverPort;
	vars["interval"] = BrokerSettings.updateInterval;
	vars["token"] = BrokerSettings.token;
	vars["gpio12"] = BrokerSettings.gpio12Id;
	vars["gpio13"] = BrokerSettings.gpio13Id;
	vars["gpio14"] = BrokerSettings.gpio14Id;
	vars["gpio4"] = BrokerSettings.gpio4Id;

	response.sendTemplate(tmpl);
}

void onAjaxGpio(HttpRequest &request, HttpResponse &response)
{
	DEBUG_PRINT("GPIO: " + request.getQueryParameter("gpio"));
	DEBUG_PRINT("STATE: " + request.getQueryParameter("state"));

	int gpio = request.getQueryParameter("gpio").toInt();
	digitalWrite(gpio, request.getQueryParameter("state") == "true"?HIGH:LOW);

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["state"] = (bool)digitalRead(gpio);
	json["gpio"] = gpio;
	// send response request
	response.sendJsonObject(stream);

	// websocket broadcast io status
	String st = digitalRead(gpio)?"1":"0";
	String msg = "{ \"Type\": 2, \"gpio\": " + request.getQueryParameter("gpio") + ", \"status\": " + st  + " }";
	sendSocketClient(msg);

	String token;
	BrokerSettings.load();
	if (mqtt != NULL){
		switch (gpio){
			case 12: token = BrokerSettings.gpio12Id;
				break;
			case 13: token = BrokerSettings.gpio13Id;
				break;
			case 14: token = BrokerSettings.gpio14Id;
				break;
			case 4: token = BrokerSettings.gpio4Id;
				break;
		}
		st = digitalRead(gpio)?"on":"off";
		msg = "{\"properties\":[{ \"Type\": 2, \"token\": \"" +token + "\", \"status\": \"" + st  + "\" }]}";
		mqtt->publish("device/" + BrokerSettings.user_name + "/" + BrokerSettings.token+"/status", msg);

	}
}

void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void wsMessageReceived(WebSocket& socket, const String& message)
{
	Serial.printf("WebSocket message received:\r\n%s\r\n", message.c_str());
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");
	// Start FTP server
	ftp = new FTPServer();
	ftp->listen(21);
	ftp->addUser("admin", "123"); // FTP account
}

void startMQTT()
{
	BrokerSettings.load();
	if (BrokerSettings.active == true){
		mqttStatus = true;
		sendSocketClient("{ \"Type\": 3, \"status\": false }");
		mqtt = new MqttClient(BrokerSettings.serverHost, BrokerSettings.serverPort, onMessageReceived);
		// Run MQTT client
		mqtt->connect(deviceId, BrokerSettings.user_name, BrokerSettings.password);
		mqtt->subscribe(getTopic);
		mqtt->subscribe(setTopic);
		// Start publishing loop
		procTimer.initializeMs(BrokerSettings.updateInterval * 1000, publishMessage).start(); // every 5 seconds
	}
}

void startWebServer()
{
	server = new HttpServer();
	server->listen(80);
	server->addPath("/", onIndex);
	server->addPath("/network", onNetworkConfig);
	server->addPath("/config", onBrokerConfig);
	server->addPath("/ajax/gpio", onAjaxGpio);
	server->setDefaultHandler(onFile);

	// Web Sockets configuration
	server->enableWebSockets(true);
	server->setWebSocketMessageHandler(wsMessageReceived);

	if (!WifiAccessPoint.isEnabled()){
		Serial.println("\r\n=== WEB SERVER STARTED ===");
		Serial.println(WifiStation.getIP());
		Serial.println("==========================\r\n");
	}else{
		Serial.println("\r\n=== WEB SERVER STARTED ===");
		Serial.println(WifiAccessPoint.getIP());
		Serial.println("==========================\r\n");
	}
}

void startServers()
{
	DEBUG_PRINT("System Ready...");
	if (WifiAccessPoint.isEnabled()){
		Serial.println("\r\n=== AP ADDRESS ===");
		Serial.println(WifiAccessPoint.getIP());
		Serial.println("======================\r\n");
		startFTP();
		startWebServer();
		lcdStartServers();
	}
}

void connectOk()
{
	startFTP();
	startWebServer();
	startMQTT();
#ifdef LCD_DISPLAY
	lcdConnectOk();
#endif
#ifndef LCD_DISPLAY
	statusTimer.initializeMs(1000, blink).start();
#endif
}

void networkScanCompleted(bool succeeded, BssList list)
{
	if (succeeded)
	{
		for (int i = 0; i < list.count(); i++)
			if (!list[i].hidden && list[i].ssid.length() > 0)
				networks.add(list[i]);
	}
	networks.sort([](const BssInfo& a, const BssInfo& b){ return b.rssi - a.rssi; } );

	DEBUG_PRINT("Network Scan Completed");
}

void enablerAccessPoint()
{
	DeviceSettings.load();
	WifiAccessPoint.enable(true);
	WifiAccessPoint.config(DeviceSettings.ssid + "-" + String(getChipId()), DeviceSettings.password, AUTH_OPEN);
}

// Will be called when WiFi station timeout was reached
void connectFail()
{
	DEBUG_PRINT("I'm NOT CONNECTED. Need help :(");
#ifndef LCD_DISPLAY
	statusTimer.initializeMs(100, blink).start();
#else
	lcdStartServers();
#endif
	NetworkSettings.Delete();
}

void IRAM_ATTR interruptHandler()
{
	if (digitalRead(INT_PIN) == LOW){
		pressed = millis();
	}
	if (digitalRead(INT_PIN) == HIGH){
		if ((millis() - pressed) > 500){
			pressed = millis();
		}
	}
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(false); // Debug output to serial

#ifdef LCD_DISPLAY
	attachInterrupt(INT_PIN, interruptHandler, CHANGE);
	Wire.pins(2, 0);
	lcd.begin(16, 2);
	lcd.backlight();
	lcdSystemStarting();
#endif

	pinMode(12, OUTPUT);
	pinMode(13, OUTPUT);
	pinMode(14, OUTPUT);
	pinMode(4, OUTPUT);
#ifndef LCD_DISPLAY
	pinMode(CONF_PIN, OUTPUT);
#endif
	allOutputOff();
	BrokerSettings.load();

	getTopic = "devices/" + BrokerSettings.user_name + "/" + BrokerSettings.token+"/get";
	setTopic = "devices/"+ BrokerSettings.user_name + "/" + BrokerSettings.token+"/set";

#ifndef WIFI_SSID
	//attachInterrupt(INT_PIN, interruptHandler, CHANGE);
	NetworkSettings.load();
	if (NetworkSettings.exist())
	{
		WifiStation.config(NetworkSettings.ssid, NetworkSettings.password);
		if (!NetworkSettings.dhcp && !NetworkSettings.ip.isNull())
			WifiStation.setIP(NetworkSettings.ip, NetworkSettings.netmask, NetworkSettings.gateway);
		WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start

		WifiAccessPoint.enable(false);
	}else{
		//procTimer.initializeMs(5000, startSmartConfig).startOnce();
#ifndef LCD_DISPLAY
		statusTimer.initializeMs(100, blink).start();
#endif
		enablerAccessPoint();
	}
#else
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.waitConnection(connectOk, 20, connectFail);
#endif

	WifiStation.startScan(networkScanCompleted);
	// Start AP for configuration
	//DeviceSettings.load();
	//WifiAccessPoint.enable(false);
	//WifiAccessPoint.config(DeviceSettings.ssid + "-" + String(getChipId()), DeviceSettings.password, AUTH_OPEN);
	// Run WEB server on system ready
	System.onReady(startServers);
	// Run our method when station was connected to AP (or not connected)
	//WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start
}
