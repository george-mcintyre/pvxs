/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <pvxs/log.h>
#include <fstream>

#include "yajl_alloc.h"
#include "yajl_parse.h"

#include "groupconfigprocessor.h"
#include "iocshcommand.h"
#include "dbentry.h"
#include "groupprocessorcontext.h"
#include "yajlcallbackhandler.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.search");

/**
 * Parse group configuration that has been defined in db configuration files.
 * This involves extracting info fields named "Q:Group" from the database configuration
 * and converting them to GroupPv configuration information.
 */
void GroupConfigProcessor::parseDbConfig() {
	runOnPvxsServer([this](IOCServer* pPvxsServer) {
		// process info blocks named Q:Group to get group configuration
		DBEntry dbEntry;
		for (long status = dbFirstRecordType(dbEntry); !status; status = dbNextRecordType(dbEntry)) {
			for (status = dbFirstRecord(dbEntry); !status; status = dbNextRecord(dbEntry)) {
				const char* jsonGroupDefinition = infoField(dbEntry, "Q:group");
				if (jsonGroupDefinition != nullptr) {
					auto& dbRecordName(dbEntry->precnode->recordname);
					log_debug_printf(_logname, "%s: info(Q:Group, ...\n", dbRecordName);

					try {
						parseConfigString(pPvxsServer, dbRecordName, jsonGroupDefinition);
						if (!pPvxsServer->groupProcessingWarnings.empty()) {
							std::cout << dbRecordName << ": warning(s) from info(\"Q:group\", ..." << std::endl
							          << pPvxsServer->groupProcessingWarnings.c_str();
						}
					} catch (std::exception& e) {
						std::cerr << dbRecordName << ": Error parsing info(\"Q:group\", ..." << std::endl
						          << e.what();
					}
				}
			}
		}
	});
}

/**
 * Parse group definitions from the collected list of group definition files.
 *
 * Get the list of group files configured on the iocServer and convert them to GroupPv configurations.
 */
void GroupConfigProcessor::parseConfigFiles() {
	runOnPvxsServer([this](IOCServer* pPvxsServer) {
		// get list of group files to load
		auto& groupDefinitionFiles = pPvxsServer->groupDefinitionFiles;

		// For each file load the configuration file
		std::list<std::string>::iterator it;
		for (it = groupDefinitionFiles.begin(); it != groupDefinitionFiles.end(); ++it) {
			auto& groupDefinitionFile = it;
			auto groupDefinitionFileName = it->c_str();

			// Get contents of group definition file
			std::ifstream jsonGroupDefinitionStream(groupDefinitionFileName, std::ifstream::in);
			if (!jsonGroupDefinitionStream.is_open()) {
				std::cerr << "Error opening \"" << groupDefinitionFileName << "\"" << std::endl;
				continue;
			}
			std::stringstream buffer;
			buffer << jsonGroupDefinitionStream.rdbuf();
			auto jsonGroupDefinition = buffer.str();

			log_debug_printf(_logname, "Process dbGroup file \"%s\"\n", groupDefinitionFileName);

			try {
				parseConfigString(pPvxsServer, nullptr, jsonGroupDefinition.c_str());
				if (!pPvxsServer->groupProcessingWarnings.empty()) {
					std::cerr << "warning(s) from group definition file \"" << groupDefinitionFileName << "\""
					          << std::endl << pPvxsServer->groupProcessingWarnings << std::endl;
				}
			} catch (std::exception& e) {
				std::cerr << "Error reading group definition file \"" << groupDefinitionFileName << "\""
				          << std::endl << e.what() << std::endl;
			}
		}
	});
}

/**
 * Parse the given json string as a group definition part for the given dbRecord
 * name and extract group pv definition into the given iocServer
 *
 * @param iocServer the iocServer to add the group definition info to
 * @param dbRecordName the name of the dbRecord
 * @param jsonGroupDefinition the given json string representing a group definition
 */
void GroupConfigProcessor::parseConfigString(IOCServer* iocServer, const char* dbRecordName,
		const char* jsonGroupDefinition) {
#ifndef EPICS_YAJL_VERSION
	yajl_parser_config parserConfig;
	memset(&parserConfig, 0, sizeof(parserConfig));
	parserConfig.allowComments = 1;
	parserConfig.checkUTF8 = 1;
#endif

	// Convert the json string to a stream to be passed to the json parser
	std::istringstream jsonGroupDefinitionStream(jsonGroupDefinition);

	std::string channelPrefix;

	if (dbRecordName) {
		channelPrefix = dbRecordName;
		channelPrefix += '.';
	}

	// Create a parser context for the parser
	GroupProcessorContext parserContext(channelPrefix, iocServer);

#ifndef EPICS_YAJL_VERSION
	YajlHandler handle(yajl_alloc(&yajlParserCallbacks, &parserConfig, NULL, &parserContext));
#else

	// Create a callback handler for the parser
	YajlCallbackHandler callbackHandler(yajl_alloc(&yajlParserCallbacks, nullptr, &parserContext));

	// Configure the parser with the handler and some options (allow comments)
	yajl_config(callbackHandler, yajl_allow_comments, 1);
#endif

	// Parse the json stream for group definitions using the configured parser
	if (!yajlParseHelper(jsonGroupDefinitionStream, callbackHandler)) {
		throw std::runtime_error(parserContext.msg);
	}
}

/**
 * To process key part of json nodes.  This will be followed by a boolean, integer, block, or null
 *
 * @param parserContext the parser context
 * @param key the key
 * @param keyLength the length of the key
 * @return non-zero if successful
 */
int GroupConfigProcessor::processKey(void* parserContext, const unsigned char* key, size_t keyLength) {
	return GroupConfigProcessor::yajlProcess(parserContext, [&key, &keyLength](GroupProcessorContext* self) {
		if (keyLength == 0 && self->depth != 2) {
			throw std::runtime_error("empty group or key name not allowed");
		}

		std::string name((const char*)key, keyLength);

		if (self->depth == 1) {
			self->groupName.swap(name);
		} else if (self->depth == 2) {
			self->field.swap(name);
		} else if (self->depth == 3) {
			self->key.swap(name);
		} else {
			throw std::logic_error("Malformed json group definition: too many nesting levels");
		}

		return 1;
	});
}

/**
 * To process null json nodes
 *
 * @param parserContext the parser context
 * @return non-zero if successful
 */
int GroupConfigProcessor::processNull(void* parserContext) {
	return GroupConfigProcessor::yajlProcess(parserContext, [](GroupProcessorContext* self) {
		self->assign(epics::pvData::AnyScalar());
		return 1;
	});
}

/**
 * To process boolean json nodes
 *
 * @param parserContext the parser context
 * @param booleanValue the boolean value
 * @return non-zero if successful
 */
int GroupConfigProcessor::processBoolean(void* parserContext, int booleanValue) {
	return GroupConfigProcessor::yajlProcess(parserContext, [&booleanValue](GroupProcessorContext* self) {
		self->assign(epics::pvData::AnyScalar(epics::pvData::boolean(booleanValue)));
		return 1;
	});
}

/**
 * To process integer json nodes
 *
 * @param parserContext the parser context
 * @param integerVal the integer value
 * @return non-zero if successful
 */
int GroupConfigProcessor::processInteger(void* parserContext, long long integerVal) {
	return GroupConfigProcessor::yajlProcess(parserContext, [&integerVal](GroupProcessorContext* self) {
		self->assign(epics::pvData::AnyScalar(epics::pvData::int64(integerVal)));
		return 1;
	});
}

/**
 * To process double json nodes
 *
 * @param parserContext the parser context
 * @param doubleVal the double value
 * @return non-zero if successful
 */
int GroupConfigProcessor::processDouble(void* parserContext, double doubleVal) {
	return GroupConfigProcessor::yajlProcess(parserContext, [&doubleVal](GroupProcessorContext* self) {
		self->assign(epics::pvData::AnyScalar(doubleVal));
		return 1;
	});
}

/**
 * To process string json nodes
 *
 * @param parserContext the parser context
 * @param stringVal the string value
 * @param stringLen the string length
 * @return non-zero if successful
 */
int GroupConfigProcessor::processString(void* parserContext, const unsigned char* stringVal, size_t stringLen) {
	return GroupConfigProcessor::yajlProcess(parserContext, [&stringVal, &stringLen](GroupProcessorContext* self) {
		std::string val((const char*)stringVal, stringLen);
		self->assign(epics::pvData::AnyScalar(val));
		return 1;
	});
}

/**
 * To start processing new json blocks
 *
 * @param parserContext the parser context
 * @return non-zero if successful
 */
int GroupConfigProcessor::processStartBlock(void* parserContext) {
	return GroupConfigProcessor::yajlProcess(parserContext, [](GroupProcessorContext* self) {
		self->depth++;
		if (self->depth > 3) {
			throw std::runtime_error("Group field def. can't contain Object (too deep)");
		}
		return 1;
	});
}

/**
 * To end processing the current json block
 *
 * @param parserContext the parser context
 * @return non-zero if successful
 */
int GroupConfigProcessor::processEndBlock(void* parserContext) {
	return GroupConfigProcessor::yajlProcess(parserContext, [](GroupProcessorContext* self) {
		assert(self->key.empty()); // cleared in assign()

		if (self->depth == 3) {
			self->key.clear();
		} else if (self->depth == 2) {
			self->field.clear();
		} else if (self->depth == 1) {
			self->groupName.clear();
		} else {
			throw std::logic_error("Internal error in json parser: invalid depth");
		}
		self->depth--;

		return 1;
	});
}

/**
 * Helper function to wrap processing of json lexical elements.
 * All exceptions are caught and translated into processing context messages
 *
 * @param parserContext the parser context
 * @param pFunction the lambda to call to process the given element
 * @return the value returned from the lambda function
 */
int
GroupConfigProcessor::yajlProcess(void* parserContext, const std::function<int(GroupProcessorContext*)>& pFunction) {
	auto* pContext = (GroupProcessorContext*)parserContext;
	int returnValue = -1;
	try {
		returnValue = pFunction(pContext);
	} catch (std::exception& e) {
		if (pContext->msg.empty()) {
			pContext->msg = e.what();
		}
	}
	return returnValue;
}

/**
 * Get the info field string from the given dbEntry for the given key.
 * If the key is not found then return the given default value.
 *
 * @param dbEntry the given dbEntry
 * @param key the key to get the info field for
 * @param defaultValue the default value to return in case its not found
 * @return the string for the info key
 */
const char* GroupConfigProcessor::infoField(DBEntry& dbEntry, const char* key, const char* defaultValue) {
	// If field not found then return default value
	if (dbFindInfo(dbEntry, key)) {
		return defaultValue;
	}

	// Otherwise return the info string
	return dbGetInfoString(dbEntry);
}

/**
 * Checks to see if there are trailing comments at the end of the line.
 * Throws an exception if there are
 *
 * @param line the line to check
 */
void GroupConfigProcessor::checkForTrailingCommentsAtEnd(const std::string& line) {
	size_t idx = line.find_first_not_of(" \t\n\r");
	if (idx != line.npos) {
		// trailing comments not allowed
		throw std::runtime_error("Trailing comments are not allowed");
	}
}

/**
 * Parse the given stream as a json group definition using the given json parser handler
 *
 * @param jsonGroupDefinitionStream the given json group definition stream
 * @param handle the handler
 * @return true if successful
 */
bool GroupConfigProcessor::yajlParseHelper(std::istream& jsonGroupDefinitionStream, yajl_handle handle) {
	unsigned linenum = 0;
#ifndef EPICS_YAJL_VERSION
	bool done = false;
#endif

	std::string line;
	while (std::getline(jsonGroupDefinitionStream, line)) {
		linenum++;

#ifndef EPICS_YAJL_VERSION
		if(done) {
			check_trailing(line);
			continue;
		}
#endif

		// Parse the next line from the json group definition
		yajl_status status = yajl_parse(handle, (const unsigned char*)line.c_str(), line.size());

		switch (status) {
		case yajl_status_ok: {
			size_t consumed = yajl_get_bytes_consumed(handle);

			if (consumed < line.size()) {
				checkForTrailingCommentsAtEnd(line.substr(consumed));
			}

#ifndef EPICS_YAJL_VERSION
			done = true;
#endif
			break;
		}
		case yajl_status_client_canceled:
			return false;
#ifndef EPICS_YAJL_VERSION
			case yajl_status_insufficient_data:
			// continue with next line
			break;
#endif
		case yajl_status_error: {
			std::ostringstream errorMessage;
			unsigned char* raw = yajl_get_error(handle, 1, (const unsigned char*)line.c_str(), line.size());
			if (!raw) {
				errorMessage << "Unknown error on line " << linenum;
			} else {
				try {
					errorMessage << "Error on line " << linenum << " : " << (const char*)raw;
				} catch (...) {
					yajl_free_error(handle, raw);
					throw;
				}
				yajl_free_error(handle, raw);
			}
			throw std::runtime_error(errorMessage.str());
		}
		}
	}

	if (!jsonGroupDefinitionStream.eof() || jsonGroupDefinitionStream.bad()) {
		std::ostringstream msg;
		msg << "I/O error after line " << linenum;
		throw std::runtime_error(msg.str());

#ifndef EPICS_YAJL_VERSION
		} else if(!done) {
		switch(yajl_parse_complete(handle)) {
#else
	} else {
		switch (yajl_complete_parse(handle)) {
#endif
		case yajl_status_ok:
			break;
		case yajl_status_client_canceled:
			return false;
#ifndef EPICS_YAJL_VERSION
			case yajl_status_insufficient_data:
			throw std::runtime_error("unexpected end of input");
#endif
		case yajl_status_error:
			throw std::runtime_error("Error while completing parsing");
		}
	}

	return true;
}

} // ioc
} // pvxs
