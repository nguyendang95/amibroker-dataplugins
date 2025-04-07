#include "sstream"
#include "Windows.h"
#include <iomanip>

unsigned int StringToUInt(const char* str) {
    std::string strTimeInfo(str);
    std::stringstream strstream{ strTimeInfo };
    unsigned int uintTimeInfo;
    strstream >> uintTimeInfo;
    return uintTimeInfo;
}

unsigned int ToDateNum(const tm timeInfo) {
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%y%m%d", &timeInfo);
    unsigned int uintDateNum = StringToUInt(buffer);
    return uintDateNum;
}

unsigned int ToTimeNum(const tm timeInfo) {
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%H%M", &timeInfo);
    unsigned int uintTimeNum = StringToUInt(buffer);
    return uintTimeNum;
}

static char* VtBstrToChar(const BSTR bstr) {
    if (!bstr) return NULL;
    size_t len = SysStringLen(bstr);
    char* str = new char[len + 1];
    if (!str || wcstombs(str, bstr, len + 1) < 0) {
        delete[] str;
        return NULL;
    }
    return str;
}

static char* WcharToChar(const wchar_t* wstr) {
    size_t len;
    if (!wstr || !(len = wcslen(wstr))) return NULL;
    char* pStr = new char[len];
    if (!pStr || wcstombs(pStr, wstr, len) < 0) {
        delete[] pStr;
        return NULL;
    }
    return pStr;
}

tm EpochTimeToLocalTime(const long long input) {
    time_t epochTime = input;
    struct tm timeInfo {};
    localtime_s(&timeInfo, &epochTime);
    return timeInfo;
}

time_t LocalTimeToEpochTime(tm* input) {
    time_t timeInfo{};
    timeInfo = mktime(input);
    return timeInfo;
}

bool ParseDateTime(const std::string& strDateTime, std::tm& tmDateTime) {
    std::istringstream ss(strDateTime);
    ss >> std::get_time(&tmDateTime, "%Y-%m-%d %H:%M:%S");
    return !ss.fail();
}