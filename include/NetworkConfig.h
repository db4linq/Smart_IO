/*
 * NetworkConfig.h
 *
 *  Created on: Jul 24, 2558 BE
 *      Author: admin
 */

#include <SmingCore/SmingCore.h>

#ifndef APP_NETWORKCONFIG_H_
#define APP_NETWORKCONFIG_H_

#define APP_SETTINGS_FILE ".networks.conf" // leading point for security reasons :)

struct NetwotkSettingsStorage
{
	String ssid;
	String password;

	bool dhcp = true;

	IPAddress ip;
	IPAddress netmask;
	IPAddress gateway;

	void load()
	{
		DynamicJsonBuffer jsonBuffer;
		if (exist())
		{
			int size = fileGetSize(APP_SETTINGS_FILE);
			char* jsonString = new char[size + 1];
			fileGetContent(APP_SETTINGS_FILE, jsonString, size + 1);
			JsonObject& root = jsonBuffer.parseObject(jsonString);

			JsonObject& network = root["network"];
			ssid = network["ssid"].toString();
			password = network["password"].toString();

			dhcp = network["dhcp"];

			ip = network["ip"].toString();
			netmask = network["netmask"].toString();
			gateway = network["gateway"].toString();

			delete[] jsonString;
		}else{
			ssid = "";
			password = "";
		}
	}

	void save()
	{
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();

		JsonObject& network = jsonBuffer.createObject();
		root["network"] = network;
		network["ssid"] = ssid.c_str();
		network["password"] = password.c_str();

		network["dhcp"] = dhcp;

		// Make copy by value for temporary string objects
		network.addCopy("ip", ip.toString());
		network.addCopy("netmask", netmask.toString());
		network.addCopy("gateway", gateway.toString());

		//TODO: add direct file stream writing
		fileSetContent(APP_SETTINGS_FILE, root.toJsonString());
	}

	void Delete(){
		fileDelete(APP_SETTINGS_FILE);
	}

	bool exist() { return fileExist(APP_SETTINGS_FILE); }
};

static NetwotkSettingsStorage NetworkSettings;

#endif /* APP_NETWORKCONFIG_H_ */
