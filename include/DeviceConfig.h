/*
 * DeviceConfig.h
 *
 *  Created on: Jul 24, 2558 BE
 *      Author: admin
 */
#include <SmingCore/SmingCore.h>

#ifndef APP_DEVICECONFIG_H_
#define APP_DEVICECONFIG_H_


#define DEVICE_SETTINGS_FILE ".device.conf" // leading point for security reasons :)

struct DeviceSettingsStorage
{
	String ip;
	String ssid;
	String password;

	void load()
	{
		DynamicJsonBuffer jsonBuffer;
		if (exist())
		{
			int size = fileGetSize(DEVICE_SETTINGS_FILE);
			char* jsonString = new char[size + 1];
			fileGetContent(DEVICE_SETTINGS_FILE, jsonString, size + 1);
			JsonObject& root = jsonBuffer.parseObject(jsonString);

			JsonObject& device = root["device"];
			ip = device["ip"].toString();
			ssid = device["ssid"].toString();
			password = device["password"].toString();

			delete[] jsonString;
		}else{
			ip = "192.168.4.1";
			ssid = "ESP8266";
			password = "";
		}
	}

	void save()
	{
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();

		JsonObject& device = jsonBuffer.createObject();
		root["device"] = device;
		device["ip"] = ip.c_str();
		device["ssid"] = ssid.c_str();
		device["password"] = password.c_str();

		//TODO: add direct file stream writing
		fileSetContent(DEVICE_SETTINGS_FILE, root.toJsonString());
	}
	bool exist() { return fileExist(DEVICE_SETTINGS_FILE); }
};

static DeviceSettingsStorage DeviceSettings;

#endif /* APP_DEVICECONFIG_H_ */
