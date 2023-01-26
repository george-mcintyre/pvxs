/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <map>

#include <dbChannel.h>
#include <pvxs/log.h>
#include <pvxs/nt.h>

#include <yajl_alloc.h>
#include <yajl_parse.h>

#include "groupconfigprocessor.h"
#include "iocshcommand.h"
#include "dbentry.h"
#include "grouppv.h"
#include "groupprocessorcontext.h"
#include "yajlcallbackhandler.h"
#include "iocsource.h"

namespace pvxs {
namespace ioc {

DEFINE_LOGGER(_logname, "pvxs.ioc.group.processor");

/**
 * Parse group configuration that has been defined in db configuration files.
 * This involves extracting info fields named "Q:Group" from the database configuration
 * and converting them to GroupPv configuration information.
 */
void GroupConfigProcessor::parseDbConfig() {
	// process info blocks named Q:Group to get group configuration
	DBEntry dbEntry;
	for (long status = dbFirstRecordType(dbEntry); !status; status = dbNextRecordType(dbEntry)) {
		for (status = dbFirstRecord(dbEntry); !status; status = dbNextRecord(dbEntry)) {
			const char* jsonGroupDefinition = infoField(dbEntry, "Q:group");
			if (jsonGroupDefinition != nullptr) {
				auto& dbRecordName(dbEntry->precnode->recordname);
				log_debug_printf(_logname, "%s: info(Q:Group, ...\n", dbRecordName);

				try {
					parseConfigString(jsonGroupDefinition, dbRecordName);
					if (!groupProcessingWarnings.empty()) {
						std::cerr << dbRecordName << ": warning(s) from info(\"Q:group\", ..." << std::endl
						          << groupProcessingWarnings.c_str();
					}
				} catch (std::exception& e) {
					fprintf(stderr, "%s: Error parsing info(\"Q:group\", ...\n%s", dbRecordName, e.what());
				}
			}
		}
	}
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
			auto groupDefinitionFileName = it->c_str();

			// Get contents of group definition file
			std::ifstream jsonGroupDefinitionStream(groupDefinitionFileName, std::ifstream::in);
			if (!jsonGroupDefinitionStream.is_open()) {
				fprintf(stderr, "Error opening \"%s\"\n", groupDefinitionFileName);
				continue;
			}
			std::stringstream buffer;
			buffer << jsonGroupDefinitionStream.rdbuf();
			auto jsonGroupDefinition = buffer.str();

			log_debug_printf(_logname, "Process dbGroup file \"%s\"\n", groupDefinitionFileName);

			try {
				parseConfigString(jsonGroupDefinition.c_str());
				if (!groupProcessingWarnings.empty()) {
					fprintf(stderr, "warning(s) from group definition file \"%s\"\n%s\n",
							groupDefinitionFileName, groupProcessingWarnings.c_str());
				}
			} catch (std::exception& e) {
				fprintf(stderr, "Error reading group definition file \"%s\"\n%s\n",
						groupDefinitionFileName, e.what());
			}
		}
	});
}

/**
 * After the group configuration has been read into pvxs::ioc::IOCServer::groupConfig
 * this function is called to evaluate it and create groups in pvxs::ioc::IOCServer::groupConfig
 */
void GroupConfigProcessor::configureGroups() {
	for (auto& groupIterator: groupConfigMap) {
		const std::string& groupName = groupIterator.first;
		const GroupConfig& groupConfig = groupIterator.second;

		try {
			// If the configured group name is the same as a record name then ignore it
			if (dbChannelTest(groupName.c_str()) == 0) {
				fprintf(stderr, "%s : Error: Group name conflicts with record name.  Ignoring...\n",
						groupName.c_str());
				continue;
			}

			// Create group when it is first referenced
			auto&& currentGroup = groupPvMap[groupName];

			// If the structure ID is not already set then set it
			if (!groupConfig.structureId.empty()) {
				groupPvMap[groupName].structureId = groupConfig.structureId;
			}

			// for each field configure the fields
			for (auto&& fieldEntry: groupConfig.groupFields) {
				const std::string& fieldName = fieldEntry.first;
				const GroupFieldConfig& fieldConfig = fieldEntry.second;

				if (currentGroup.fieldMap.count(fieldName)) {
					fprintf(stderr, "%s.%s Warning: ignoring duplicate mapping %s\n",
							groupName.c_str(), fieldName.c_str(), fieldConfig.channel.c_str());
					continue;
				}

				// For updating the capacity for each array field
				currentGroup.fields.emplace_back();
				auto& currentField = currentGroup.fields.back();
				currentField.channel = fieldConfig.channel;
				currentField.name = fieldName;
				currentField.structureId = fieldConfig.structureId;
				currentField.putOrder = fieldConfig.putOrder;
				currentField.type = fieldConfig.type;

				currentGroup.fieldMap[fieldName] = (size_t)-1;      // placeholder  see below

				log_debug_printf(_logname, "  pvxs map '%s.%s' <-> '%s'\n",
						groupName.c_str(),
						fieldName.c_str(),
						currentField.channel.c_str());

				if (!fieldConfig.trigger.empty()) {
					Triggers triggers;
					std::string trigger;
					std::stringstream splitter(fieldConfig.trigger);
					currentGroup.hasTriggers = true;

					while (std::getline(splitter, trigger, ',')) {
						triggers.insert(trigger);
					}
					currentGroup.triggerMap[fieldName] = triggers;
				}
			}

			if (groupConfig.atomic_set) {
				TriState atomicity = groupConfig.atomic ? TriState::True : TriState::False;

				if (currentGroup.atomic != TriState::Unset && currentGroup.atomic != atomicity) {
					fprintf(stderr, "%s  Warning: pvxs atomic setting inconsistent\n", groupName.c_str());
				}

				currentGroup.atomic = atomicity;

				log_debug_printf(_logname, "  pvxs atomic '%s' %s\n",
						groupName.c_str(),
						currentGroup.atomic ? "YES" : "NO");
			}

		} catch (std::exception& e) {
			fprintf(stderr, "Error configuring group \"%s\" : %s\n", groupName.c_str(), e.what());
		}
	}

	// re-sort fields to ensure the shorter names appear first
	for (auto&& mapEntry: groupPvMap) {
		auto& currentGroup = mapEntry.second;
		std::sort(currentGroup.fields.begin(), currentGroup.fields.end());
		currentGroup.fieldMap.clear();

		for (size_t i = 0, N = currentGroup.fields.size(); i < N; i++) {
			currentGroup.fieldMap[currentGroup.fields[i].name] = i;
		}
	}
}

/**
 * Resolve all trigger references to the specified fields
 */
void GroupConfigProcessor::resolveTriggers() {
	// For all groups
	for (auto&& mapEntry: groupPvMap) {
		auto& groupName = mapEntry.first;
		auto& currentGroup = mapEntry.second;

		// If it has triggers
		if (currentGroup.hasTriggers) {
			// Then for all triggers
			for (auto&& triggerMapEntry: currentGroup.triggerMap) {
				const std::string& field = triggerMapEntry.first;
				Triggers& targets = triggerMapEntry.second;

				if (currentGroup.fieldMap.count(field) == 0) {
					fprintf(stderr, "Error: Group \"%s\" defines triggers from nonexistent field \"%s\" \n",
							groupName.c_str(), field.c_str());
					continue;
				}

				auto& sourceMemberIndex = currentGroup.fieldMap[field];
				auto& sourceMember = currentGroup.fields[sourceMemberIndex];

				log_debug_printf(_logname, "  pvxs trigger '%s.%s'  -> ", groupName.c_str(), field.c_str());

				// For all of this trigger's targets
				for (auto&& target: targets) {
					// If the target is star then map to all fields
					if (target == "*") {
						for (auto& targetedField: currentGroup.fields) {
							if (!targetedField.channel.empty()) {
								sourceMember.triggers.insert(targetedField.name);
								log_debug_printf(_logname, "%s, ", targetedField.name.c_str());
							}
						}
					} else {
						// otherwise map to the specific target if it exists
						if (currentGroup.fieldMap.count(target) == 0) {
							fprintf(stderr, "Error: Group \"%s\" defines triggers to nonexistent field \"%s\" \n",
									groupName.c_str(), target.c_str());
							continue;
						}
						auto& targetMemberIndex = currentGroup.fieldMap[target];
						auto& targetMember = currentGroup.fields[targetMemberIndex];

						// And if it references a PV
						if (targetMember.channel.empty()) {
							log_debug_printf(_logname, "<ignore: %s>, ", targetMember.name.c_str());
						} else {
							sourceMember.triggers.insert(targetMember.name);
							log_debug_printf(_logname, "%s, ", targetMember.name.c_str());
						}
					}
				}
				log_debug_printf(_logname, "%s\n", "");
			}
		} else {
			// If no trigger specified for this group then set all fields to trigger themselves
			log_debug_printf(_logname, "  pvxs default triggers for '%s'\n", groupName.c_str());

			for (auto&& field: currentGroup.fields) {
				if (!field.channel.empty()) {
					field.triggers.insert(field.name);  // default is self trigger
				}
			}
		}
	}
}

/**
 * Process the configured groups to create the final IOCGroups containing PVStructure templates and all the
 * infrastructure needed to respond to PVAccess requests linked to the underlying IOC database
 *
 * 1. Collects builds IOCGroups and IOCGroupFields from GroupPvs
 * 2. Build PVStructures for each IOCGroup and discard those w/o a dbChannel
 * 3. Build the lockers for each group field based on their triggers
 */
void GroupConfigProcessor::createGroups() {
	runOnPvxsServer([this](IOCServer* pPvxsServer) {
		auto& groupMap = pPvxsServer->groupMap;


		// First pass: Create groups and get array capacities
		for (auto& groupPvMapEntry: groupPvMap) {
			auto& groupName = groupPvMapEntry.first;
			auto& groupPv = groupPvMapEntry.second;
			try {
				if (groupMap.count(groupName) != 0) {
					throw std::runtime_error("Group name already in use");
				}
				// Create group or access existing group
				auto& group = groupMap[groupName];

				// Set basic group information
				group.name = groupName;
				group.atomicPutGet = groupPv.atomic != False;
				group.atomicMonitor = groupPv.hasTriggers;

				// Initialise the given group's fields from the given group PV configuration
				initialiseGroupFields(group, groupPv);
			} catch (std::exception& e) {
				fprintf(stderr, "%s: Error Group not created: %s\n", groupName.c_str(), e.what());
			}
		}

		// Second Pass: assemble group PV structure definitions
		for (auto& groupPvMapEntry: groupPvMap) {
			auto& groupName = groupPvMapEntry.first;
			auto& groupPv = groupPvMapEntry.second;
			try {
				auto& group = groupMap[groupName];
				// Initialise the given group's value type
				initialiseGroupValueTemplates(group, groupPv);
			} catch (std::exception& e) {
				fprintf(stderr, "%s: Error Group not created: %s\n", groupName.c_str(), e.what());
			}
		}
	});
}

/**
 * Initialise the given group's fields from the given configuration.
 *
 * The group configuration contains a set of fields.  These fields define the structure of the group.
 * Dot notation (a.b.c) define substructures with subfields, and bracket notation (a[1].b) define
 * structure arrays. Each configuration reference points to a database record (channel).
 * This function uses the configuration to define the group fields that are required
 * to link to the specified database records.  This means that one group field is created for each
 * referenced database record.
 *
 * @param group the IOC group to store fields->channel mappings
 * @param groupPv the groupPv configuration we're reading group information from
 */
void GroupConfigProcessor::initialiseGroupFields(IOCGroup& group, const GroupPv& groupPv) {
	// Reserve enough space for fields with channels
	group.fields.reserve(groupPv.channelCount());

	// for each field with channels
	for (auto& field: groupPv.fields) {
		if (!field.channel.empty()) {
			// Create field in group
			group.fields.emplace_back(field.name, field.channel);
		}
	}
}

/**
 * Initialise the given group's fields and value type from the given group PV configuration.
 * The group's field names and channels are set - this calls dbChannelCreate to create the dbChannel for each field.
 * It also creates the top level PVStructure for the group and stores it in valueType.
 *
 * @param group the IOC group we're setting
 * @param groupPv the groupPv configuration we're reading from
 */
void GroupConfigProcessor::initialiseGroupValueTemplates(IOCGroup& group, const GroupPv& groupPv) {
	using namespace pvxs::members;
	TypeDef groupType = (groupPv.structureId.empty()) ?
	                    TypeDef(TypeCode::Struct, {}) :
	                    TypeDef(TypeCode::Struct, groupPv.structureId, {});

	// Metadata first because it could have to be inserted in the top level
	for (auto& field: groupPv.fields) {
		if (!field.channel.empty()) {
			auto& groupField = group[field.name];
			auto& groupFieldName = groupField.name;
			auto& type = field.type;

			auto pdbChannel = (dbChannel*)groupField.channel;

			// Scalars map to nt types
			if (type == "meta") {
				groupField.isMeta = true;
				TypeDef valueType ;
				buildMetaValueType(valueType, groupField, pdbChannel, groupPv.structureId);
				if (groupFieldName.empty()) {
					groupType = valueType;
				} else {
					groupType += { valueType.as(groupFieldName) };
				}
			}
		}
	}

	// for each field create the value template required
	for (auto& field: groupPv.fields) {
		if (!field.channel.empty()) {
			auto& groupField = group[field.name];
			auto& groupFieldName = groupField.name;
			auto& type = field.type;

			auto pdbChannel = (dbChannel*)groupField.channel;

			// Scalars map to nt types
			if (type == "proc") {
				groupField.allowProc = true;
			} else if (type == "meta") { // already processed
			} else {
				TypeDef valueType;
				if (type.empty() || type == "scalar") {
					buildScalarValueType(valueType, groupField, pdbChannel);
				} else if (type == "plain") {
					buildPlainValueType(valueType, groupField, pdbChannel);
				} else if (type == "any") {
					buildAnyScalarValueType(valueType, groupField, pdbChannel);
				} else if (type == "structure") {
					buildStructureValueType(valueType, groupField, pdbChannel);
				} else {
					throw std::runtime_error(std::string("Unknown +type=") + type);
				}
				groupType += { valueType.as(groupFieldName) };
			}
		}
	}

	// Roll up value templates into group value template
	auto groupValueTemplate = groupType.create();
	group.valueTemplate = std::move(groupValueTemplate);
}

/**
 * Parse the given json string as a group definition part for the given dbRecord
 * name and extract group pv definition into the given iocServer
 *
 * @param dbRecordName the name of the dbRecord
 * @param jsonGroupDefinition the given json string representing a group definition
 */
void GroupConfigProcessor::parseConfigString(const char* jsonGroupDefinition, const char* dbRecordName) {
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
	GroupProcessorContext parserContext(channelPrefix, this);

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
		self->assign(Value());
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
		auto value = pvxs::TypeDef(TypeCode::Bool).create();
		value = booleanValue;
		self->assign(value);
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
		auto value = pvxs::TypeDef(TypeCode::Int64).create();
		value = (int64_t)integerVal;
		self->assign(value);
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
		auto value = pvxs::TypeDef(TypeCode::Float64).create();
		value = doubleVal;
		self->assign(value);
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
		auto value = pvxs::TypeDef(TypeCode::String).create();
		value = val;
		self->assign(value);
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

void GroupConfigProcessor::buildScalarValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel) {
	auto& fieldName = groupField.fieldName;
	assert(!fieldName.empty()); // Must not call with empty field name

	// Get the type for the leaf
	auto leafCode(IOCSource::getChannelValueType(pdbChannel, true));
	TypeDef leaf;

	// Create the leaf
	auto dbfType = dbChannelFinalFieldType(pdbChannel);
	if (dbfType == DBF_ENUM) {
		leaf = nt::NTEnum{}.build();
	} else {
		bool display = true;
		bool control = (dbfType != DBF_STRING);
		bool valueAlarm = (dbfType != DBF_STRING);
		leaf = nt::NTScalar{ leafCode, display, control, valueAlarm }.build();
	}

	getFieldTypeDefinition(valueType, groupField.fieldName, leaf);
}

void GroupConfigProcessor::buildPlainValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel) {
	auto& fieldName = groupField.fieldName;
	assert(!fieldName.empty()); // Must not call with empty field name

	// Get the type for the leaf
	auto leafCode(IOCSource::getChannelValueType(pdbChannel, true));
	TypeDef leaf(leafCode);
	getFieldTypeDefinition(valueType, groupField.fieldName, leaf);
}

void GroupConfigProcessor::buildAnyScalarValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel) {
	auto& fieldName = groupField.fieldName;
	assert(!fieldName.empty()); // Must not call with empty field name

	// Get the type for the leaf
	auto leafCode(IOCSource::getChannelValueType(pdbChannel, true));
	TypeDef leaf(leafCode);
	if (groupField.isArray) {
		TypeDef leafUnion(TypeCode::UnionA, { leaf.as("value") });
		getFieldTypeDefinition(valueType, groupField.fieldName, leafUnion);
	} else {
		TypeDef leafUnion(TypeCode::Union, { leaf.as("value") });
		getFieldTypeDefinition(valueType, groupField.fieldName, leafUnion);
	}
}

void GroupConfigProcessor::buildStructureValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel) {
	auto& fieldName = groupField.fieldName;
	assert(!fieldName.empty()); // Must not call with empty field name

	// Get the type for the leaf
	auto leafCode(IOCSource::getChannelValueType(pdbChannel, true));
	TypeDef leaf(leafCode);
	if (groupField.isArray) {
		TypeDef leafUnion(TypeCode::StructA, { leaf.as("value") });
		getFieldTypeDefinition(valueType, groupField.fieldName, leafUnion);
	} else {
		TypeDef leafUnion(TypeCode::Struct, { leaf.as("value") });
		getFieldTypeDefinition(valueType, groupField.fieldName, leafUnion);
	}
}

void
GroupConfigProcessor::buildMetaValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel,
		const std::string& id) {
	using namespace pvxs::members;
	// if this is a meta field then create standard meta fields
	auto metaSourceCode(IOCSource::getChannelValueType(pdbChannel, true));
	short dbfType = dbChannelFinalFieldType(pdbChannel);
	getMetadataTypeDefinition(valueType, dbfType, metaSourceCode, groupField.name.empty() ? id : "");
	getFieldTypeDefinition(valueType, groupField.fieldName, valueType);
}

void
GroupConfigProcessor::getMetadataTypeDefinition(TypeDef& metaValueType, short databaseType, TypeCode typeCode,
		const std::string& id) {
	using namespace pvxs::members;

	if (id.empty()) {
		metaValueType = TypeDef(TypeCode::Struct, {});
	} else {
		metaValueType = TypeDef(TypeCode::Struct, id, {});
	}

	metaValueType += {
			Struct("alarm", "alarm_t", {
					Int32("severity"),
					Int32("status"),
					String("message"),
			}),
			nt::TimeStamp{}.build().as("timeStamp"),
	};

	const bool isnumeric = databaseType == DBF_FLOAT || databaseType == DBF_DOUBLE ||
			databaseType == DBF_INT64 || databaseType == DBF_LONG || databaseType == DBF_SHORT ||
			databaseType == DBF_UINT64 || databaseType == DBF_ULONG || databaseType == DBF_USHORT;

	if (isnumeric) {
		metaValueType += {
				Struct("display", {
						Member(typeCode, "limitLow"),
						Member(typeCode, "limitHigh"),
						String("description"),
						String("units"),
				}),
				Struct("control", {
						Member(typeCode, "limitLow"),
						Member(typeCode, "limitHigh"),
						Member(typeCode, "minStep"),
				}),
				Struct("valueAlarm", {
						Bool("active"),
						Member(typeCode, "lowAlarmLimit"),
						Member(typeCode, "lowWarningLimit"),
						Member(typeCode, "highWarningLimit"),
						Member(typeCode, "highAlarmLimit"),
						Int32("lowAlarmSeverity"),
						Int32("lowWarningSeverity"),
						Int32("highWarningSeverity"),
						Int32("highAlarmSeverity"),
						Float64("hysteresis"),
				}),
		};

	} else if (databaseType == DBF_STRING) {
		metaValueType += {
				Struct("display", {
						String("description"),
						String("units"),
				}),
		};
	}
}

/**
 * Get the type definition for the given field name based on the referenced channel.
 * Use the field name path to construct the appropriate field type definition.
 * Note: assumes that the field name has at least one component.
 *
 * @param fieldName the field name
 * @param leaf the leaf node to place in the type definition
 * @return the type definition for the given field name
 */
void GroupConfigProcessor::getFieldTypeDefinition(TypeDef& valueType, const IOCGroupFieldName& fieldName,
		TypeDef& leaf) {
	auto subComponentName = !fieldName.empty() ? fieldName[fieldName.size() - 1].name : "";
	auto isLeaf = true;

	// Make up the full structure starting from the leaf
	for (auto fieldNameComponentNumber = fieldName.size();
	     fieldNameComponentNumber > 0;
	     fieldNameComponentNumber--) {
		auto&& component = fieldName[fieldNameComponentNumber - 1];
		if (isLeaf) {
			isLeaf = false;
			valueType = leaf;
		} else if (!subComponentName.empty()) {
			if (component.isArray()) {
				valueType = TypeDef(TypeCode::StructA, { valueType.as(subComponentName) });
			} else {
				valueType = TypeDef(TypeCode::Struct, { valueType.as(subComponentName) });
			}
		}
		subComponentName = component.name;
	}
}

} // ioc
} // pvxs
