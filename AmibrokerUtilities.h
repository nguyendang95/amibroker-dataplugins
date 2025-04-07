#pragma once
#include "sstream"

unsigned int ToTimeNum(const tm timeInfo);
unsigned int ToDateNum(const tm timeInfo);
bool ParseDateTime(const std::string& strDateTime, std::tm& tmDateTime);
time_t LocalTimeToEpochTime(tm* input);
tm EpochTimeToLocalTime(const long long input);
static char* WcharToChar(const wchar_t* wstr);
static char* VtBstrToChar(const BSTR bstr);
unsigned int StringToUInt(const char* str);
