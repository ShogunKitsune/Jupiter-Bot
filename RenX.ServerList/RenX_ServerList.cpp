/**
 * Copyright (C) 2016 Jessica James.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Written by Jessica James <jessica.aj@outlook.com>
 */

#include "Jupiter/IRC_Client.h"
#include "Jupiter/INIFile.h"
#include "Jupiter/HTTP.h"
#include "Jupiter/HTTP_QueryString.h"
#include "HTTPServer.h"
#include "RenX_Core.h"
#include "RenX_Server.h"
#include "RenX_ServerList.h"
#include "RenX_Functions.h"

using namespace Jupiter::literals;

static STRING_LITERAL_AS_NAMED_REFERENCE(CONTENT_TYPE_APPLICATION_JSON, "application/json");

Jupiter::String jsonify(const Jupiter::ReadableString &in_str)
{
	const char *ptr = in_str.ptr();
	size_t str_length = in_str.size();
	Jupiter::String result(str_length);

	while (str_length != 0)
	{
		if (*ptr == '\\') // backslash
		{
			result += '\\';
			result += '\\';
		}
		else if (*ptr == '\"') // quotation
		{
			result += '\\';
			result += '\"';
		}
		else if (*ptr < 0x20) // control characters
			result += Jupiter::StringS::Format("\\u00%x", *ptr);
		else if ((*ptr & 0x80) != 0) // UTF-8 sequence; copy to bypass above processing
		{
			result += *ptr;

			if ((*ptr & 0x40) != 0)
			{
				// this is a 2+ byte sequence

				if ((*ptr & 0x20) != 0)
				{
					// this is a 3+ byte sequence

					if ((*ptr & 0x10) != 0)
					{
						// this is a 4 byte sequnce
						result += *++ptr;
					}

					result += *++ptr;
				}

				result += *++ptr;
			}
		}
		else // Character in standard ASCII table
			result += *ptr;

		++ptr;
		--str_length;
	}

	return result;
}

RenX_ServerListPlugin::RenX_ServerListPlugin()
{
	RenX_ServerListPlugin::web_hostname = Jupiter::IRC::Client::Config->get(this->name, "Hostname"_jrs, ""_jrs);
	RenX_ServerListPlugin::web_path = Jupiter::IRC::Client::Config->get(this->name, "Path"_jrs, "/"_jrs);
	RenX_ServerListPlugin::server_list_page_name = Jupiter::IRC::Client::Config->get(this->name, "ServersPageName"_jrs, "servers.json"_jrs);
	RenX_ServerListPlugin::server_page_name = Jupiter::IRC::Client::Config->get(this->name, "ServerPageName"_jrs, "server.json"_jrs);

	/** Initialize content */
	Jupiter::HTTP::Server &server = getHTTPServer();

	// Server list page
	Jupiter::HTTP::Server::Content *content = new Jupiter::HTTP::Server::Content(RenX_ServerListPlugin::server_list_page_name, handle_server_list_page);
	content->language = &Jupiter::HTTP::Content::Language::ENGLISH;
	content->type = &CONTENT_TYPE_APPLICATION_JSON;
	content->charset = &Jupiter::HTTP::Content::Type::Text::Charset::UTF8;
	content->free_result = false;
	server.hook(RenX_ServerListPlugin::web_hostname, RenX_ServerListPlugin::web_path, content);

	// Server page (GUIDs)
	content = new Jupiter::HTTP::Server::Content(RenX_ServerListPlugin::server_page_name, handle_server_page);
	content->language = &Jupiter::HTTP::Content::Language::ENGLISH;
	content->type = &CONTENT_TYPE_APPLICATION_JSON;
	content->charset = &Jupiter::HTTP::Content::Type::Text::Charset::UTF8;
	content->free_result = true;
	server.hook(RenX_ServerListPlugin::web_hostname, RenX_ServerListPlugin::web_path, content);

	this->updateServerList();
}

RenX_ServerListPlugin::~RenX_ServerListPlugin()
{
	Jupiter::HTTP::Server &server = getHTTPServer();
	server.remove(RenX_ServerListPlugin::web_hostname, RenX_ServerListPlugin::web_path, RenX_ServerListPlugin::server_list_page_name);
	server.remove(RenX_ServerListPlugin::web_hostname, RenX_ServerListPlugin::web_path, RenX_ServerListPlugin::server_page_name);
}

Jupiter::ReadableString *RenX_ServerListPlugin::getServerListJSON()
{
	return std::addressof(RenX_ServerListPlugin::server_list_json);
}

const char *json_bool_as_cstring(bool in)
{
	if (in)
		return "true";
	return "false";
}

Jupiter::StringS server_as_json(const RenX::Server *server)
{
	Jupiter::String server_json_block(128);

	Jupiter::String server_name = jsonify(server->getName());
	Jupiter::String server_map = jsonify(server->getMap().name);
	Jupiter::String server_version = jsonify(server->getGameVersion());

	server_json_block.format(R"json({"Name":"%.*s","Current Map":"%.*s","Bots":%u,"Players":%u,"Game Version":"%.*s","Variables":{"Mine Limit":%d,"bSteamRequired":%s,"bPrivateMessageTeamOnly":%s,"bPassworded":%s,"bAllowPrivateMessaging":%s,"Player Limit":%d,"Vehicle Limit":%d,"bAutoBalanceTeams":%s,"bSpawnCrates":%s,"CrateRespawnAfterPickup":%f,"Time Limit":%d},"Port":%u,"IP":"%.*s")json",
		server_name.size(), server_name.ptr(),
		server_map.size(), server_map.ptr(),
		server->getBotCount(),
		server->players.size() - server->getBotCount(),
		server_version.size(), server_version.ptr(),
		server->getMineLimit(),
		json_bool_as_cstring(server->isSteamRequired()),
		json_bool_as_cstring(server->isPrivateMessageTeamOnly()),
		json_bool_as_cstring(server->isPassworded()),
		json_bool_as_cstring(server->isPrivateMessagingEnabled()),
		server->getPlayerLimit(),
		server->getVehicleLimit(),
		json_bool_as_cstring(server->isAutoBalanceEnabled()),
		json_bool_as_cstring(server->isCratesEnabled()),
		server->getCrateRespawnDelay(),
		server->getTimeLimit(),
		server->getPort(),
		server->getSocketHostname().size(), server->getSocketHostname().ptr());


	// Level Rotation
	/*if (server->maps.size() != 0)
	{
		server_json_block += ",\"Levels\":["_jrs;

		server_json_block += "{\"Name\":\""_jrs;
		server_json_block += server->maps.get(0)->name;
		server_json_block += "\",\"GUID\":\""_jrs;
		server_json_block += RenX::formatGUID(*server->maps.get(0));
		server_json_block += "\"}"_jrs;

		for (size_t index = 1; index != server->maps.size(); ++index)
		{
			server_json_block += ",{\"Name\":\""_jrs;
			server_json_block += server->maps.get(index)->name;
			server_json_block += "\",\"GUID\":\""_jrs;
			server_json_block += RenX::formatGUID(*server->maps.get(index));
			server_json_block += "\"}"_jrs;
		}

		server_json_block += "]"_jrs;
	}

	// Mutators
	if (server->mutators.size() != 0)
	{
		server_json_block += ",\"Mutators\": ["_jrs;

		server_json_block += "{\"Name\":\""_jrs;
		server_json_block += *server->mutators.get(0);
		server_json_block += "\"}"_jrs;

		for (size_t index = 1; index != server->mutators.size(); ++index)
		{
			server_json_block += ",{\"Name\":\""_jrs;
			server_json_block += *server->mutators.get(index);
			server_json_block += "\"}"_jrs;
		}

		server_json_block += "]"_jrs;
	}*/

	server_json_block += "}"_jrs;

	return server_json_block;
}

Jupiter::StringS server_as_hr_json(const RenX::Server *server)
{
		Jupiter::String server_json_block(128);

		Jupiter::String server_name = jsonify(server->getName());
		Jupiter::String server_map = jsonify(server->getMap().name);
		Jupiter::String server_version = jsonify(server->getGameVersion());

		server_json_block.format(R"json({
		"Name": "%.*s",
		"Current Map": "%.*s",
		"Bots": %u,
		"Players": %u,
		"Game Version": "%.*s",
		"Variables": {
			"Mine Limit": %d,
			"bSteamRequired": %s,
			"bPrivateMessageTeamOnly": %s,
			"bPassworded": %s,
			"bAllowPrivateMessaging": %s,
			"Player Limit": %d,
			"Vehicle Limit": %d,
			"bAutoBalanceTeams": %s,
			"bSpawnCrates": %s,
			"CrateRespawnAfterPickup": %f,
			"Time Limit": %d
		},
		"Port": %u,
		"IP": "%.*s")json",

			server_name.size(), server_name.ptr(),
			server_map.size(), server_map.ptr(),
			server->getBotCount(),
			server->players.size() - server->getBotCount(),
			server_version.size(), server_version.ptr(),

			server->getMineLimit(),
			json_bool_as_cstring(server->isSteamRequired()),
			json_bool_as_cstring(server->isPrivateMessageTeamOnly()),
			json_bool_as_cstring(server->isPassworded()),
			json_bool_as_cstring(server->isPrivateMessagingEnabled()),
			server->getPlayerLimit(),
			server->getVehicleLimit(),
			json_bool_as_cstring(server->isAutoBalanceEnabled()),
			json_bool_as_cstring(server->isCratesEnabled()),
			server->getCrateRespawnDelay(),
			server->getTimeLimit(),

			server->getPort(),
			server->getSocketHostname().size(), server->getSocketHostname().ptr());


		// Level Rotation
		if (server->maps.size() != 0)
		{
		server_json_block += ",\n\t\t\"Levels\": ["_jrs;

		server_json_block += "\n\t\t\t{\n\t\t\t\t\"Name\": \""_jrs;
		server_json_block += server->maps.get(0)->name;
		server_json_block += "\",\n\t\t\t\t\"GUID\": \""_jrs;
		server_json_block += RenX::formatGUID(*server->maps.get(0));
		server_json_block += "\"\n\t\t\t}"_jrs;

		for (size_t index = 1; index != server->maps.size(); ++index)
		{
		server_json_block += ",\n\t\t\t{\n\t\t\t\t\"Name\": \""_jrs;
		server_json_block += server->maps.get(index)->name;
		server_json_block += "\",\n\t\t\t\t\"GUID\": \""_jrs;
		server_json_block += RenX::formatGUID(*server->maps.get(index));
		server_json_block += "\"\n\t\t\t}"_jrs;
		}

		server_json_block += "\n\t\t]"_jrs;
		}

		// Mutators
		if (server->mutators.size() != 0)
		{
		server_json_block += ",\n\t\t\"Mutators\": ["_jrs;

		server_json_block += "\n\t\t\t{\n\t\t\t\t\"Name\": \""_jrs;
		server_json_block += *server->mutators.get(0);
		server_json_block += "\"\n\t\t\t}"_jrs;

		for (size_t index = 1; index != server->mutators.size(); ++index)
		{
		server_json_block += ",\n\t\t\t{\n\t\t\t\t\"Name\": \""_jrs;
		server_json_block += *server->mutators.get(index);
		server_json_block += "\"\n\t\t\t}"_jrs;
		}

		server_json_block += "\n\t\t]"_jrs;
		}

		server_json_block += "\n\t}"_jrs;

		return server_json_block;
}

void RenX_ServerListPlugin::addServerToServerList(const RenX::Server *server)
{
	// append to server_list_json

	if (RenX_ServerListPlugin::server_list_json.isEmpty())
	{
		RenX_ServerListPlugin::server_list_json = '[';
		RenX_ServerListPlugin::server_list_json += server_as_json(server);
		RenX_ServerListPlugin::server_list_json += ']';
	}
	else
	{
		RenX_ServerListPlugin::server_list_json.truncate(1); // remove trailing ']'.
		RenX_ServerListPlugin::server_list_json += ',';
		RenX_ServerListPlugin::server_list_json += server_as_json(server);
		RenX_ServerListPlugin::server_list_json += ']';
	}
}

void RenX_ServerListPlugin::updateServerList()
{
	Jupiter::ArrayList<RenX::Server> servers = RenX::getCore()->getServers();
	size_t index = 0;
	RenX::Server *server;

	// regenerate server_list_json

	RenX_ServerListPlugin::server_list_json = '[';

	while (index != servers.size())
	{
		server = servers.get(index);
		if (server->isConnected() && server->isFullyConnected())
		{
			RenX_ServerListPlugin::server_list_json += server_as_json(server);
			++index;
			break;
		}
		++index;
	}
	while (index != servers.size())
	{
		server = servers.get(index);
		if (server->isConnected() && server->isFullyConnected())
		{
			RenX_ServerListPlugin::server_list_json += ',';
			RenX_ServerListPlugin::server_list_json += server_as_json(server);
		}
		++index;
	}

	RenX_ServerListPlugin::server_list_json += ']';
}

void RenX_ServerListPlugin::RenX_OnServerFullyConnected(RenX::Server *server)
{
	Jupiter::String server_json_block(256);

	this->addServerToServerList(server);

	// add to individual listing

	server_json_block = '{';

	if (server->maps.size() != 0)
	{
		server_json_block += ",\"Levels\":["_jrs;

		server_json_block += "{\"Name\":\""_jrs;
		server_json_block += jsonify(server->maps.get(0)->name);
		server_json_block += "\",\"GUID\":\""_jrs;
		server_json_block += RenX::formatGUID(*server->maps.get(0));
		server_json_block += "\"}"_jrs;

		for (size_t index = 1; index != server->maps.size(); ++index)
		{
			server_json_block += ",{\"Name\":\""_jrs;
			server_json_block += jsonify(server->maps.get(index)->name);
			server_json_block += "\",\"GUID\":\""_jrs;
			server_json_block += RenX::formatGUID(*server->maps.get(index));
			server_json_block += "\"}"_jrs;
		}

		server_json_block += ']';
	}

	// Mutators
	if (server->mutators.size() != 0)
	{
		server_json_block += ",\"Mutators\":["_jrs;

		server_json_block += "{\"Name\":\""_jrs;
		server_json_block += jsonify(*server->mutators.get(0));
		server_json_block += "\"}"_jrs;

		for (size_t index = 1; index != server->mutators.size(); ++index)
		{
			server_json_block += ",{\"Name\":\""_jrs;
			server_json_block += jsonify(*server->mutators.get(index));
			server_json_block += "\"}"_jrs;
		}

		server_json_block += ']';
	}

	server_json_block += '}';

	server->varData.set(this->name, "j"_jrs, server_json_block);
}

void RenX_ServerListPlugin::RenX_OnServerDisconnect(RenX::Server *server, RenX::DisconnectReason)
{
	this->updateServerList();

	// remove from individual listing
	server->varData.remove(this->name, "j"_jrs);
}

void RenX_ServerListPlugin::RenX_OnJoin(RenX::Server *, const RenX::PlayerInfo *)
{
	this->updateServerList();
}

void RenX_ServerListPlugin::RenX_OnPart(RenX::Server *, const RenX::PlayerInfo *)
{
	this->updateServerList();
}

// Plugin instantiation and entry point.
RenX_ServerListPlugin pluginInstance;

Jupiter::ReadableString *handle_server_list_page(const Jupiter::ReadableString &)
{
	return pluginInstance.getServerListJSON();
}

Jupiter::ReadableString *handle_server_page(const Jupiter::ReadableString &query_string)
{
	Jupiter::HTTP::HTMLFormResponse html_form_response(query_string);
	Jupiter::ReferenceString address;
	int port = 0;
	RenX::Server *server;

	// parse form data

	if (html_form_response.table.size() < 2)
		return new Jupiter::ReferenceString();

	if (html_form_response.table.size() != 0)
	{
		address = html_form_response.table.get("ip"_jrs, address);
		port = html_form_response.table.getInt("port"_jrs, port);
	}

	// search for server
	Jupiter::ArrayList<RenX::Server> servers = RenX::getCore()->getServers();
	size_t index = 0;

	while (true)
	{
		if (index == servers.size())
			return new Jupiter::ReferenceString();

		server = servers.get(index);
		if (server->getSocketHostname().equals(address) && server->getPort() == port)
			break;

		++index;
	}

	// return server data
	return new Jupiter::ReferenceString(server->varData.get(pluginInstance.getName(), "j"_jrs));
}

extern "C" __declspec(dllexport) Jupiter::Plugin *getPlugin()
{
	return &pluginInstance;
}