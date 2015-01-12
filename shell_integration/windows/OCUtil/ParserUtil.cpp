/**
 *  Copyright (c) 2000-2013 Liferay, Inc. All rights reserved.
 *  
 *  This library is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by the Free
 *  Software Foundation; either version 2.1 of the License, or (at your option)
 *  any later version.
 *  
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 *  details.
 */

#include "ParserUtil.h"
#include "UtilConstants.h"

#include <iostream>

using namespace std;

bool ParserUtil::GetItem(const wchar_t* item, const wstring* message, wstring* result)
{
	size_t start = message->find(item, 0);

	if(start == string::npos)
	{
		return false;
	}

	size_t end = message->find(COLON, start);

	if(end == string::npos)
	{
		return false;
	}

	//Move to next character after :
	end += 1;

	wchar_t c = message->at(end);

	//Move to the next character, which is the start of the value
	end += 1;
	
	if(c == '[')
	{
		return GetList(end - 1, message, result);
	}
	else
	{
		return GetValue(end, message, result);
	}
}

bool ParserUtil::GetList(size_t start, const wstring* message, wstring* result)
{
	size_t end = start + 1;

	int openBraceCount = 1;

	while(openBraceCount > 0)
	{
		size_t closeBraceLocation = message->find(CLOSE_BRACE, end);
		size_t openBraceLocation = message->find(OPEN_BRACE, end);

		if(closeBraceLocation < openBraceLocation)
		{
			openBraceCount--;
			end = closeBraceLocation + 1;
		}
		else if(openBraceLocation < closeBraceLocation)
		{
			openBraceCount++;
			end = openBraceLocation + 1;
		}

	}
	
	size_t length = end - start;

	return GetString(start, end, message, result);
}

size_t ParserUtil::GetNextStringItemInList(const wstring* message, size_t start, wstring* result)
{
	size_t end = string::npos;
	size_t commaLocation = message->find(COMMA, start);

	if(commaLocation == string::npos)
	{
		end = message->find(CLOSE_BRACE, start);
		if(end == string::npos)
		{
			end = message->length();
		}
		else
		{
			end = end - 1;
		}
	}
	else
	{
		end = commaLocation - 1;
	}

	if(!GetString(start + 2, end, message, result))
	{
		return string::npos;
	}

	return end + 2;
}

size_t ParserUtil::GetNextOCItemInList(const wstring* message, size_t start, wstring* result)
{
	size_t end = message->find(OPEN_CURLY_BRACE, start) + 1;

	int openBraceCount = 1;

	while(openBraceCount > 0)
	{
		size_t closeBraceLocation = message->find(CLOSE_CURLY_BRACE, end);
		size_t openBraceLocation = message->find(OPEN_CURLY_BRACE, end);

		if(closeBraceLocation < openBraceLocation)
		{
			openBraceCount--;
			end = closeBraceLocation + 1;
		}
		else if(openBraceLocation < closeBraceLocation)
		{
			openBraceCount++;
			end = openBraceLocation + 1;
		}
	}

	size_t length = end - start;

	if(!GetString(start, end, message, result))
	{
		return string::npos;
	}

	return end;
}

bool ParserUtil::GetValue(size_t start, const wstring* message, wstring* result)
{
	if(message->at(start - 1) == '\"')
	{
		size_t end = message->find(QUOTE, start);
		return GetString(start, end, message, result);
	}
	else
	{
		start = start - 1;

		size_t end = message->find(COMMA, start);
		
		result->append(message->substr(start, end-start));
	}

	return true;
}

bool ParserUtil::GetString(size_t start, size_t end, const wstring* message, wstring* result)
{
	if(end == string::npos)
	{
		return false;
	}

	size_t length = end - start;

	if(length > 0)
	{
		result->append(message->substr(start, length));
	}
	else
	{
		result->append(L"");
	}

	
	return true;
}

bool ParserUtil::IsList(wstring* message)
{
	wchar_t c = message->at(0);
	
	if(c == '[')
	{
		return true;
	}

	return false;
}

bool ParserUtil::ParseJsonList(wstring* message, vector<wstring*>* items)
{

	size_t currentLocation = message->find(OPEN_BRACE, 0);

	while(currentLocation < message->size())
	{
		wstring* item = new wstring();

		currentLocation = ParserUtil::GetNextStringItemInList(message, currentLocation, item);

		if(currentLocation == string::npos)
		{
			return false;
		}

		items->push_back(item);
	}

	return true;
}

bool ParserUtil::ParseOCList(wstring* message, vector<wstring*>* items)
{

	size_t currentLocation = message->find(OPEN_CURLY_BRACE, 0);

	while(currentLocation < message->size())
	{
		wstring* item = new wstring();

		currentLocation = ParserUtil::GetNextOCItemInList(message, currentLocation, item);

		if(currentLocation == string::npos)
		{
			return false;
		}

		items->push_back(item);
	}

	return true;
}

bool ParserUtil::ParseOCMessageList(wstring* message, vector<OCMessage*>* messages)
{
	vector<wstring*>* items = new vector<wstring*>();

	if(!ParseOCList(message, items))
	{
		return false;
	}

	for(vector<wstring*>::iterator it = items->begin(); it != items->end(); it++)
	{
		wstring* temp = *it;

		OCMessage* message = new OCMessage();
		message->InitFromMessage(temp);

		messages->push_back(message);
	}

	return true;
}

bool ParserUtil::SerializeList(std::vector<std::wstring>* list, std::wstring* result, bool escapeQuotes)
{
	if(result == 0)
	{
		return false;
	}

	result->append(OPEN_BRACE);

	for(vector<wstring>::iterator it = list->begin(); it != list->end(); it++)
	{
		wstring value = *it;

		if(escapeQuotes)
		{
			result->append(BACK_SLASH);
		}

		result->append(QUOTE);
		result->append(value.c_str());

		if(escapeQuotes)
		{
			result->append(BACK_SLASH);
		}

		result->append(QUOTE);
		result->append(COMMA);
	}

	//Erase last comma
	result->erase(result->size() - 1, 1);

	result->append(CLOSE_BRACE);

	return true;
}

bool ParserUtil::SerializeMessage(std::map<std::wstring*, std::wstring*>* arguments, std::wstring* result, bool escapeQuotes)
{
	if(result == 0)
	{
		return false;
	}

	result->append(OPEN_CURLY_BRACE);

	for(map<wstring*, wstring*>::iterator it = arguments->begin(); it != arguments->end(); it++)
	{
		wstring key = *it->first;
		wstring value = *it->second;

		if(escapeQuotes)
		{
			result->append(BACK_SLASH);
		}

		result->append(QUOTE);
		result->append(key.c_str());

		if(escapeQuotes)
		{
			result->append(BACK_SLASH);
		}

		result->append(QUOTE);
		result->append(COLON);
		result->append(value.c_str());
		result->append(COMMA);
	}

	//Erase last comma
	result->erase(result->size() - 1, 1);

	result->append(CLOSE_CURLY_BRACE);

	return true;
}

bool ParserUtil::SerializeMessage(OCMessage* OCMessage, std::wstring* result)
{
	if(result == 0)
	{
		return false;
	}

	result->append(OPEN_CURLY_BRACE);

	result->append(QUOTE);
	result->append(COMMAND);
	result->append(QUOTE);

	result->append(COLON);

	result->append(QUOTE);
	result->append(OCMessage->GetCommand()->c_str());
	result->append(QUOTE);

	result->append(COMMA);
	
	result->append(QUOTE);
	result->append(VALUE);
	result->append(QUOTE);

	result->append(COLON);
	
	if(!IsList(OCMessage->GetValue()))
	{
		result->append(QUOTE);
	}
	
	result->append(OCMessage->GetValue()->c_str());
	
	if(!IsList(OCMessage->GetValue()))
	{
		result->append(QUOTE);
	}

	result->append(CLOSE_CURLY_BRACE);

	return true;
}

