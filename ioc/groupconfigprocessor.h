/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPCONFIGPROCESSOR_H
#define PVXS_GROUPCONFIGPROCESSOR_H

#include <string>
#include <yajl_parse.h>
#include "iocserver.h"
#include "dbentry.h"
#include "groupconfig.h"
#include "grouppv.h"

namespace pvxs {
namespace ioc {

// Pre-declare context class
class GroupProcessorContext;

/**
 * Class to parse group configuration that has been defined in db configuration files.
 * This involves extracting info fields named "Q:Group" from the database configuration
 * and converting them to IOCGroups.
 */
class GroupConfigProcessor {
	GroupPvMap groupPvMap;

	/**
	 * These are the callbacks designated by yajl for its parser functions
	 * They must be defined in this order.
	 * Note that we don't use number, or arrays
	 */
	yajl_callbacks yajlParserCallbacks{
			&parserCallbackNull,
			&parserCallbackBoolean,
			&parserCallbackInteger,
			&parserCallbackDouble,
			nullptr,            // number
			&parserCallbackString,
			&parserCallbackStartBlock,
			&parserCallbackKey,
			&parserCallbackEndBlock,
			nullptr,            // start_array,
			nullptr,            // end_array,
	};

public:
	GroupConfigMap groupConfigMap;

	// Group processing warning messages if not empty
	std::string groupProcessingWarnings;

	GroupConfigProcessor() = default;

	static void checkForTrailingCommentsAtEnd(const std::string& line);
	void configureGroups();
	void createGroups();
	static const char* infoField(DBEntry& dbEntry, const char* key, const char* defaultValue = nullptr);
	static void initialiseGroupFields(IOCGroup& group, const GroupPv& groupPv);
	static void initialiseGroupValueTemplates(IOCGroup& group, const GroupPv& groupPv);
	void parseConfigFiles();
	void parseDbConfig();
	void resolveTriggers();
	static void setFieldTypeDefinition(std::vector<Member>& groupMembers, const IOCGroupFieldName& fieldName,
			const std::vector<Member>& leafMembers);
	static int yajlProcess(void* parserContext, const std::function<int(GroupProcessorContext*)>& pFunction);

private:
	static void
	addMembersForConfiguredFields(std::vector<Member>& groupMembers, IOCGroup& group, const GroupPv& groupPv);
	static void addMembersForAnyType(std::vector<Member>& groupMembers, const IOCGroupField& groupField);
	static void addMembersForMetaData(std::vector<Member>& groupMembers, const IOCGroupField& groupField);
	static void addMembersForPlainType(std::vector<Member>& groupMembers, const IOCGroupField& groupField,
			const dbChannel* pdbChannel);
	static void addMembersForScalarType(std::vector<Member>& groupMembers, const IOCGroupField& groupField,
			const dbChannel* pdbChannel);
	static void addMembersForStructureType(std::vector<Member>& groupMembers, const IOCGroupField& groupField);
	static void configureFieldTriggers(GroupPvField& groupPvField, const GroupPv& groupPv,
			const Triggers& targets, const std::string& groupName);
	static void configureGroupFields(GroupPv& groupPv, const GroupConfig& groupConfig,
			const std::string& groupName);
	static void configureGroupTriggers(GroupPv& groupPv, const std::string& groupName);
	static void configureAtomicity(const GroupConfig& groupConfig, GroupPv& groupPv, const std::string& groupName);
	void sortGroupFields();
	static void configureSelfTriggers(GroupPv& groupPv);
	static int parserCallbackBoolean(void* parserContext, int booleanValue);
	static int parserCallbackDouble(void* parserContext, double doubleVal);
	static int parserCallbackEndBlock(void* parserContext);
	static int parserCallbackInteger(void* parserContext, long long int integerVal);
	static int parserCallbackKey(void* parserContext, const unsigned char* key, size_t keyLength);
	static int parserCallbackNull(void* parserContext);
	static int parserCallbackStartBlock(void* parserContext);
	static int parserCallbackString(void* parserContext, const unsigned char* stringVal, size_t stringLen);
	void parseConfigString(const char* jsonGroupDefinition, const char* dbRecordName = nullptr);
	static void parseTriggerConfiguration(GroupPv& groupPv, const GroupFieldConfig& fieldConfig,
			const std::string& fieldName);
	static bool yajlParseHelper(std::istream& jsonGroupDefinitionStream, yajl_handle handle);
	static void initialiseDbLocker(IOCGroup& group);
	static void initialiseTriggers(IOCGroup& group, const GroupPv& groupPv);
};

} // ioc
} // pvxs

#endif //PVXS_GROUPCONFIGPROCESSOR_H

