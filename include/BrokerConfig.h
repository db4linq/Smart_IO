/*
 * BrokerConfig.h
 *
 *  Created on: Jul 24, 2558 BE
 *      Author: admin
 */

#include <SmingCore/SmingCore.h>

#ifndef APP_BROKERCONFIG_H_
#define APP_BROKERCONFIG_H_

#define BROKER_SETTINGS_FILE ".broker.conf" // leading point for security reasons :)

struct BrokerSettingsStorage
{
	bool active;
	String user_name;
	String password;
	String serverHost ;
	int serverPort;
	int updateInterval;
	String token;
	String gpio4Id;
	String gpio12Id;
	String gpio13Id;
	String gpio14Id;

	void load(){
		DynamicJsonBuffer jsonBuffer;
		if (exist())
		{
			int size = fileGetSize(BROKER_SETTINGS_FILE);
			char* jsonString = new char[size + 1];
			fileGetContent(BROKER_SETTINGS_FILE, jsonString, size + 1);
			JsonObject& root = jsonBuffer.parseObject(jsonString);

			JsonObject& broker = root["broker"];
			active = broker["active"];
			user_name = broker["user_name"].toString();
			password = broker["password"].toString();
			serverHost = broker["host"].toString();
			serverPort = broker["port"].toString().toInt();
			updateInterval = broker["interval"].toString().toInt();
			gpio12Id = broker["gpio12"].toString();
			gpio13Id = broker["gpio13"].toString();
			gpio14Id = broker["gpio14"].toString();
			gpio4Id = broker["gpio4"].toString();
			token = broker["token"].toString();

			delete[] jsonString;
		}else{
			active = false;
			user_name = "";
			password = "";
			serverHost = "27.254.150.73";
			serverPort = 1883;
			updateInterval = 5;
			token = "";
			gpio12Id = "";
			gpio13Id = "";
			gpio14Id = "";
			gpio4Id = "";
		}
	}

	void save(){
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();

		JsonObject& broker = jsonBuffer.createObject();
		root["broker"] = broker;
		broker["active"] = active;
		broker["user_name"] = user_name.c_str();
		broker["password"] = password.c_str();
		broker["host"] = serverHost.c_str();
		broker.addCopy("port", String(serverPort));
		broker.addCopy("interval", String(updateInterval));
		broker["token"] = token.c_str();
		broker["gpio12"] = gpio12Id.c_str();
		broker["gpio13"] = gpio13Id.c_str();
		broker["gpio14"] = gpio14Id.c_str();
		broker["gpio4"] = gpio4Id.c_str();
		//TODO: add direct file stream writing
		fileSetContent(BROKER_SETTINGS_FILE, root.toJsonString());
	}

	bool exist() { return fileExist(BROKER_SETTINGS_FILE); }
};

static BrokerSettingsStorage BrokerSettings;

#endif /* APP_BROKERCONFIG_H_ */
