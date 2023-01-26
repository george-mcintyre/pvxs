/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_GROUPCONFIGPROCESSOR_H
#define PVXS_GROUPCONFIGPROCESSOR_H

#include <yajl_parse.h>
#include "iocserver.h"
#include "dbentry.h"
#include "groupconfig.h"
#include "grouppv.h"

namespace pvxs {
namespace ioc {

// Pre-declare context class
class GroupProcessorContext;

class GroupConfigProcessor {
	GroupPvMap groupPvMap;
	/**
	 * These are the callbacks designated by yajl for its parser functions
	 * They must be defined in this order.
	 * Note that we don't use number, or arrays
	 */
	yajl_callbacks yajlParserCallbacks {
			&processNull,
			&processBoolean,
			&processInteger,
			&processDouble,
			nullptr,            // number
			&processString,
			&processStartBlock,
			&processKey,
			&processEndBlock,
			nullptr,            // start_array,
			nullptr,            // end_array,
	};

	static bool yajlParseHelper(std::istream& jsonGroupDefinitionStream, yajl_handle handle);
	static int processNull(void* parserContext);
	static int processBoolean(void* parserContext, int booleanValue);
	static int processInteger(void* parserContext, long long int integerVal);
	static int processDouble(void* parserContext, double doubleVal);
	static int processString(void* parserContext, const unsigned char* stringVal, size_t stringLen);
	static int processStartBlock(void* parserContext);
	static int processKey(void* parserContext, const unsigned char* key, size_t keyLength);
	static int processEndBlock(void* parserContext);
	static void buildScalarValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel);
	static void buildPlainValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel);
	static void buildAnyScalarValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel);
	static void buildStructureValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel);
	static void buildMetaValueType(TypeDef& valueType, IOCGroupField& groupField, dbChannel* pdbChannel,
			const std::string& id);

public:
	GroupConfigMap groupConfigMap;

	// Group processing warning messages if not empty
	std::string groupProcessingWarnings;

	GroupConfigProcessor() = default;
	void parseDbConfig();
	void parseConfigFiles();
	void resolveTriggers();
	void createGroups();
	void parseConfigString(const char* jsonGroupDefinition, const char* dbRecordName = nullptr);
	static void initialiseGroupFields(IOCGroup &group, const GroupPv &groupPv);
	static void initialiseGroupValueTemplates(IOCGroup& group, const GroupPv& groupPv);
	static void getFieldTypeDefinition(TypeDef& valueType, const IOCGroupFieldName& fieldName, TypeDef& leaf);
	static const char* infoField(DBEntry& dbEntry, const char* key, const char* defaultValue = nullptr);
	static int yajlProcess(void* parserContext, const std::function <int (GroupProcessorContext *)>&pFunction);
	static void checkForTrailingCommentsAtEnd(const std::string& line);
	void configureGroups();
	static void getMetadataTypeDefinition(TypeDef& metaValueType, short databaseType, TypeCode typeCode,
			const std::string& id);
};

} // ioc
} // pvxs

#endif //PVXS_GROUPCONFIGPROCESSOR_H

