/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Copyright (c) Barrot Technology Limited
 *
 * All rights reserved.
 *
---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
Module Name:
	hid_parser.h
Abstract:
	USB HID parser header file
Author:
	Y.D.X
-----------------------------------------------------------------------------*/

#ifndef _HID_PARSER_H
#define _HID_PARSER_H

#include "global.h"

/************************************************************************************************
 *                                   Data Types & Constants
 ***********************************************************************************************/
/*
 * Possible values of item tag
 */
/** Tag value indicating a long item */
#define HIDP_ITEM_TAG_LONG						15

/** Tag value of a Main item */
#define HIDP_ITEM_TAG_MAIN_INPUT				8
#define HIDP_ITEM_TAG_MAIN_OUTPUT				9
#define HIDP_ITEM_TAG_MAIN_BEGIN_COLLECTION		10
#define HIDP_ITEM_TAG_MAIN_FEATURE				11
#define HIDP_ITEM_TAG_MAIN_END_COLLECTION		12

/** Tag value of a Global item */
#define HIDP_ITEM_TAG_GLOBAL_USAGE_PAGE			0
#define HIDP_ITEM_TAG_GLOBAL_LOGICAL_MINIMUM	1
#define HIDP_ITEM_TAG_GLOBAL_LOGICAL_MAXIMUM	2
#define HIDP_ITEM_TAG_GLOBAL_PHYSICAL_MINIMUM	3
#define HIDP_ITEM_TAG_GLOBAL_PHYSICAL_MAXIMUM	4
#define HIDP_ITEM_TAG_GLOBAL_UNIT_EXPONENT		5
#define HIDP_ITEM_TAG_GLOBAL_UNIT				6
#define HIDP_ITEM_TAG_GLOBAL_REPORT_SIZE		7
#define HIDP_ITEM_TAG_GLOBAL_REPORT_ID			8
#define HIDP_ITEM_TAG_GLOBAL_REPORT_COUNT		9
#define HIDP_ITEM_TAG_GLOBAL_PUSH				10
#define HIDP_ITEM_TAG_GLOBAL_POP				11

/** Tag value of a Local item */
#define HIDP_ITEM_TAG_LOCAL_USAGE				0
#define HIDP_ITEM_TAG_LOCAL_USAGE_MINIMUM		1
#define HIDP_ITEM_TAG_LOCAL_USAGE_MAXIMUM		2
#define HIDP_ITEM_TAG_LOCAL_DESIGNATOR_INDEX	3
#define HIDP_ITEM_TAG_LOCAL_DESIGNATOR_MINIMUM	4
#define HIDP_ITEM_TAG_LOCAL_DESIGNATOR_MAXIMUM	5
#define HIDP_ITEM_TAG_LOCAL_STRING_INDEX		7
#define HIDP_ITEM_TAG_LOCAL_STRING_MINIMUM		8
#define HIDP_ITEM_TAG_LOCAL_STRING_MAXIMUM		9
#define HIDP_ITEM_TAG_LOCAL_DELIMITER			10

/** Report type value */
#define HIDP_REPORT_INPUT						1
#define HIDP_REPORT_OUTPUT						2
#define HIDP_REPORT_FEATURE						3

/** Item prefix (MSB->LSB): bTag[4] + bType[2] + bSize[2] */
#define HIDP_PREFIX_TYPE(_prf_byte)		(((_prf_byte) >> 2) & 0x03)
#define HIDP_PREFIX_TAG(_prf_byte)		(((_prf_byte) >> 4) & 0x0F)
#define HIDP_PREFIX_SIZE(_prf_byte)		((_prf_byte) & 0x03)

/* Report information */
typedef struct {
	UINT8 id;
	UINT8 type;
} thidp_ReportInfoStru;

/* General descriptor item */
typedef struct {
	UINT8 fmt_long;/* TRUE - Long format; FALSE - Short format */
	UINT8 size;
	UINT8 type;
	UINT8 tag;
	union {
		UINT8  u8;
		UINT16 u16;
		UINT32 u32;
		UINT8  *long_data;
	} data;
} thidp_ItemStru;

/* Global item: only the report_id is saved. */
typedef struct {
	UINT32		report_id;
} thidp_GlobalItemStru;

/* Item nested depth */
#define HIDP_GLOBAL_STACK_SIZE 		8
#define HIDP_COLLECTION_STACK_SIZE 	8

/* Parser context */
typedef struct {
	thidp_GlobalItemStru global;
	thidp_GlobalItemStru global_stack[HIDP_GLOBAL_STACK_SIZE];
	UINT16 global_stack_ptr;
	UINT16 collection_stack;/* Only save the Stack depth */
	UINT16 collection_stack_ptr;

	struct ListStru *list;/* thidp_ReportInfoStru */
} thidp_ParserStru;

/************************************************************************************************
 *                                   Tool Functions
 ***********************************************************************************************/
void hid_Parser_GetReports(UINT8 *desc, UINT32 size, struct ListStru *report_list);


#endif/* _HID_PARSER_H */
