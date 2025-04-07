#pragma once
#include <Windows.h>
#include "vector"
#include "string"
#include "nlohmann/json.hpp"
#include <WinHttp.h>
#include "exception"
#pragma comment(lib, "winhttp.lib")
#pragma prefast(suppress:6387, "WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET does not take any arguments.")

using namespace nlohmann;

class SubscribeRequest {
public:
    std::string Method;
    int Id = 0;
    std::vector<std::string> Params;
    SubscribeRequest(std::vector<std::string>params, std::string method, int id);
    std::string ToJson();
};

class BinanceStreamDataDetail {
public:
    tm KlineStartTime;
    tm KlineCloseTime;
    std::string Symbol;
    std::string Interval;
    unsigned int FirstTradeId;
    unsigned int LastTradeId;
    float OpenPrice;
    float ClosePrice;
    float HighPrice;
    float LowPrice;
    float BaseAssetVolume;
    unsigned int NumberOfTrades;
    bool IsThisLineClosed;
    float QuoteAssetVolume;
    float TakerBuyBaseAssetVolume;
    float TakerBuyQuoteAssetVolume;
    float Ignore;
};


class BinanceStreamData {
public:
    std::string EventType;
    tm EventTime;
    std::string Symbol;
    BinanceStreamDataDetail Data;
};

class BinanceStreamResponse {
public:
    std::string Stream;
    BinanceStreamData Data;
};

BinanceStreamData ParseBinanceStreamData(const json json);
BinanceStreamDataDetail ParseBinanceStreamDataDetail(const json json);
BinanceStreamResponse ParseBinanceStreamResponse(const json json);

class WebSocketException : public std::exception {
private:
    std::string message;
public:
    WebSocketException(const char* errMsg) : message(errMsg) {

    }
    const char* what() const throw() {
        return message.c_str();
    }
    WebSocketException() = default;
};

enum WebSocketConnectionState {
    STATE_WEB_SOCKET_CREATION_FAILED,
    STATE_WEB_SOCKET_CREATION_SUCCESSFUL,
    STATE_WEB_SOCKET_RECEIVING_RESPONSE,
    STATE_WEB_SOCKET_RESPONSE_ERROR,
    STATE_WEB_SOCKET_CONNECTION_LOST,
    STATE_WEB_SOCKET_OUT_OF_MEMORY,
    STATE_WEB_SOCKET_CONNECTION_CLOSED,
    STATE_WEB_SOCKET_NONE
};