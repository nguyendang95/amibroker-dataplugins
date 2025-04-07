#include "Windows.h"
#include "Plugin.h"
#include "AmibrokerUtilities.h"
#include "map"
#include "Binance.h"
#include "fstream"
#include "cstdlib"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

HWND hwnd;
bool exitConnection;
static std::map<std::string, BinanceStreamResponse>data;
HINTERNET hWebSocket, hSession, hConnect, hRequest;
WebSocketConnectionState WebSocketState = STATE_WEB_SOCKET_NONE;
struct RecentInfo* pRis = new RecentInfo[10];
int RecentInfoSize = 10;
int SymbolLimit = 100;

static void GrowRecentInfoIfNecessary(int i) {
    if (pRis == NULL || i >= RecentInfoSize) {
        RecentInfoSize += 200;
        pRis = (struct RecentInfo*)realloc(pRis, sizeof(struct RecentInfo) * RecentInfoSize);
        memset(pRis + RecentInfoSize - 200, 0, sizeof(struct RecentInfo) * 200);
    }
}

static struct RecentInfo* FindOrAddRecentInfo(LPCTSTR pszTicker) {
    struct RecentInfo* ri = NULL;
    if (pRis == NULL) return NULL;
    int i = 0;
    for (i = 0; pRis && i < RecentInfoSize && pRis[i].Name && pRis[i].Name[0]; i++) {
        if (!_stricmp(pRis[i].Name, (char*)pszTicker)) {
            ri = &pRis[i];
            return ri;
        }
    }
    if (i < SymbolLimit) {
        GrowRecentInfoIfNecessary(i);
        ri = &pRis[i];
        strcpy(ri->Name, (char*)pszTicker);
        ri->nStatus = 0;
    }
    return ri;
}

static struct RecentInfo* FindRecentInfo(LPCTSTR pszTicker) {
    struct RecentInfo* ri = NULL;
    for (int i = 0; pRis && i < RecentInfoSize && pRis[i].Name && pRis[i].Name[0]; i++) {
        if (!_stricmp(pRis[i].Name, (char*)pszTicker)) {
            ri = &pRis[i];
            break;
        }
    }
    return ri;
}

static bool Subscribe(HINTERNET hWebSocket, std::vector<std::string>symbols) {
    DWORD dwError;
    static int id = 0;
    std::vector<std::string>newSymbolList;
    for (size_t i = 0; i < symbols.size(); i++) {
        std::string symbol(symbols[i]);
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char c) { return std::tolower(c); });
        id++;
        newSymbolList.push_back(std::string(symbol) + std::string("@kline_1s"));
    }
    SubscribeRequest request{ newSymbolList, "SUBSCRIBE", id };
    std::string requestJson(request.ToJson());
    dwError = WinHttpWebSocketSend(hWebSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)requestJson.c_str(), (DWORD)requestJson.size());
    if (dwError != NO_ERROR) return false;
    return true;
}

static HINTERNET ConnectToBinance(void) {
    HINTERNET WebSocket;
    BOOL fStatus;
    hSession = WinHttpOpen(L"WinHttp WebSocket", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) {
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    hConnect = WinHttpConnect(hSession, L"stream.binance.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/stream", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    fStatus = WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    if (!fStatus) {
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    fStatus = WinHttpSetOption(hRequest, WINHTTP_OPTION_USER_AGENT, (LPVOID)L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 Edg/133.0.0.0", ARRAYSIZE(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 Edg/133.0.0.0") * sizeof(WCHAR));
    if (!fStatus) {
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    fStatus = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
    if (!fStatus) {
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    fStatus = WinHttpReceiveResponse(hRequest, NULL);
    if (!fStatus) {
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    WebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
    if (!WebSocket) {
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        WebSocketState = STATE_WEB_SOCKET_CREATION_FAILED;
        return NULL;
    }
    WinHttpCloseHandle(hRequest);
    WebSocketState = STATE_WEB_SOCKET_CREATION_SUCCESSFUL;
    return WebSocket;
}

static DWORD WINAPI ReadResponse(LPVOID lpParam)
{
    HINTERNET handle = *(HINTERNET*)lpParam;
    bool connectionClosedByServer = false;
    while (!exitConnection) {
        DWORD dwBytesTransferred, dwTotalBytesTransferred = 0, dwError;
        BYTE rgbBuffer[11576]{};
        DWORD dwBufferLength = ARRAYSIZE(rgbBuffer);
        BYTE* pbCurrentBufferPointer = rgbBuffer;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE eBufferType;
        do {
            if (dwBufferLength == 0) {
                exitConnection = TRUE;
                WebSocketState = STATE_WEB_SOCKET_OUT_OF_MEMORY;
                break;
            }
            dwError = WinHttpWebSocketReceive(hWebSocket, pbCurrentBufferPointer, dwBufferLength, &dwBytesTransferred, &eBufferType);
            if (dwError != NO_ERROR) {
                exitConnection = TRUE;
                WebSocketState = STATE_WEB_SOCKET_CONNECTION_LOST;
                break;
            }
            switch (eBufferType) {
            case WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE:
                dwError = WinHttpWebSocketClose(hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
                if (dwError != NO_ERROR) {
                    exitConnection = TRUE;
                    WebSocketState = STATE_WEB_SOCKET_RESPONSE_ERROR;
                    connectionClosedByServer = true;
                    break;
                }
                connectionClosedByServer = true;
                WebSocketState = STATE_WEB_SOCKET_CONNECTION_CLOSED;
                exitConnection = TRUE;
                break;
            }
            pbCurrentBufferPointer += dwBytesTransferred;
            dwBufferLength -= dwBytesTransferred;
            dwTotalBytesTransferred += dwBytesTransferred;
            if (dwTotalBytesTransferred > dwBufferLength) {
                dwError = WinHttpWebSocketClose(hWebSocket, WINHTTP_WEB_SOCKET_CLOSE_STATUS::WINHTTP_WEB_SOCKET_MESSAGE_TOO_BIG_CLOSE_STATUS, NULL, 0);
                if (dwError != NO_ERROR) {
                    WebSocketState = STATE_WEB_SOCKET_RESPONSE_ERROR;
                    exitConnection = true;
                    break;
                }
                connectionClosedByServer = true;
                WebSocketState = STATE_WEB_SOCKET_CONNECTION_CLOSED;
                exitConnection = TRUE;
                break;
            }
        } while (eBufferType == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE);
        if (exitConnection) break;
        char* strResponse = new char[dwTotalBytesTransferred + 1];
        if (!strResponse) {
            exitConnection = true;
            WebSocketState = STATE_WEB_SOCKET_OUT_OF_MEMORY;
            break;
        }
        memcpy(strResponse, &rgbBuffer, dwTotalBytesTransferred);
        strResponse[dwTotalBytesTransferred] = (CHAR)'\0';
        nlohmann::json json = nlohmann::json::parse(strResponse);
        BinanceStreamResponse response = ParseBinanceStreamResponse(json);
        if (!response.Stream.empty()) {
            WebSocketState = STATE_WEB_SOCKET_RECEIVING_RESPONSE;
            data.insert_or_assign(response.Data.Symbol, response);
            struct RecentInfo* ri = FindOrAddRecentInfo((LPCTSTR)response.Data.Symbol.c_str());
            ri->fHigh = response.Data.Data.HighPrice;
            ri->fLow = response.Data.Data.LowPrice;
            ri->fOpen = response.Data.Data.OpenPrice;
            ri->nStructSize = sizeof(struct RecentInfo);
            ri->nDateUpdate = ToDateNum(response.Data.EventTime);
            ri->nTimeUpdate = ToTimeNum(response.Data.EventTime);
            ri->nBitmap = RI_HIGHLOW | RI_OPEN | RI_DATEUPDATE;
            ri->nStatus = RI_STATUS_UPDATE | RI_STATUS_TRADE | RI_STATUS_BARSREADY;
            SendMessage(hwnd, WM_USER_STREAMING_UPDATE, (WPARAM)response.Data.Symbol.c_str(), (LPARAM)&ri);

        }
        delete[] strResponse;
    }
    if (!connectionClosedByServer) {
        DWORD dwError = WinHttpWebSocketClose(hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
        if (dwError != NO_ERROR) {
            WebSocketState = STATE_WEB_SOCKET_RESPONSE_ERROR;
        }
        else WebSocketState = STATE_WEB_SOCKET_CONNECTION_CLOSED;
    }
    if (hWebSocket) WinHttpCloseHandle(hWebSocket);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return 0;
}

static PluginInfo pluginInfo{sizeof(PluginInfo), PLUGIN_TYPE_DATA, 1, PIDCODE('t', 'e',  's', '2'), "Binance"};

static bool PackTime(Quotation* qt, const tm timeInfo) {
    if (!qt) return false;
    qt->DateTime.Date = 0;
    qt->DateTime.PackDate.Day = timeInfo.tm_mday;
    qt->DateTime.PackDate.Month = timeInfo.tm_mon + 1;
    qt->DateTime.PackDate.Year = timeInfo.tm_year + 1900;
    qt->DateTime.PackDate.Hour = timeInfo.tm_hour;
    qt->DateTime.PackDate.Minute = timeInfo.tm_min;
    qt->DateTime.PackDate.Second = timeInfo.tm_sec;
    return true;
}
extern "C" {
    PLUGINAPI int GetPluginInfo(struct PluginInfo* pInfo) {
        *pInfo = pluginInfo;
        return TRUE;
    }

    PLUGINAPI int Init(void) {
        if (hWebSocket = ConnectToBinance()) {
            CreateThread(NULL, 0, ReadResponse, (LPVOID)hWebSocket, 0, NULL);
            std::fstream symbolList;
            if (!symbolList.good()) 
                symbolList.open(std::getenv("TMP") + std::string("\\binanceSymbolList.tmp"), std::ios::in | std::ios::out);
            else 
                symbolList.open(std::getenv("TMP") + std::string("\\binanceSymbolList.tmp"), std::ios::in);
            std::vector<std::string>subscribedSymbols;
            std::string symbol;
            while (std::getline(symbolList, symbol)) {
                subscribedSymbols.push_back(symbol);
            }
            symbolList.close();
            size_t len = subscribedSymbols.size();
            if (len > 0) Subscribe(hWebSocket, subscribedSymbols);
        }
        else {
            WebSocketState = STATE_WEB_SOCKET_CONNECTION_LOST;
        }
        return 1;
    }

    PLUGINAPI int Release(void) {
        exitConnection = true;
        if (hWebSocket) WinHttpCloseHandle(hWebSocket);
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        delete[] pRis;
        std::fstream symbolList;
        symbolList.open(std::getenv("TMP") + std::string("\\binanceSymbolList.tmp"), std::ios::out);
        if (symbolList.good()) {
            for (auto const& pair : data) {
                auto& symbol = pair.first;
                symbolList << symbol << std::endl;
            }
            symbolList.close();
        }
        return 1;
    }

    static bool PopulateBinanceStreamResponse(BinanceStreamResponse item, struct Quotation* qt) {
        if (!PackTime(qt, item.Data.EventTime)) return false;
        qt->Open = item.Data.Data.OpenPrice;
        qt->Price = item.Data.Data.ClosePrice;
        qt->High = item.Data.Data.HighPrice;
        qt->Low = item.Data.Data.LowPrice;
        qt->Volume = item.Data.Data.QuoteAssetVolume;
        return true;
    }

    PLUGINAPI int GetQuotesEx(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation* pQuotes, GQEContext* pContext) {
        static int index = 0;
        std::string strSymbol((char*)pszTicker);
        auto searchResult = data.find(strSymbol);
        if (searchResult != data.end()) {
            auto& result = searchResult->second;
            if (index = nSize) {
                memmove(&pQuotes[0], &pQuotes[1], (nSize - 1) * sizeof(Quotation));
                index--;
            }
            Quotation* qt = &pQuotes[index];
            if (PopulateBinanceStreamResponse(result, qt)) {
                index++;
                return index;
            }
        }
        else {
            size_t len = data.size();
            if (len > 0) Subscribe(hWebSocket, {strSymbol});
            return 0;
        }
        return nLastValid++;
    }

    PLUGINAPI int Notify(struct PluginNotification* pn) {
        if ((pn->nReason & REASON_DATABASE_LOADED)) {
            hwnd = pn->hMainWnd;
        }
        return 1;
    }

    PLUGINAPI int GetStatus(struct PluginStatus* status) {
        switch (WebSocketState) {
        case STATE_WEB_SOCKET_CONNECTION_CLOSED:
            status->nStatusCode = 0x20000000;
            strcpy(status->szLongMessage, "WebSocket connection has been closed.");
            strcpy(status->szShortMessage, "CLOSED");
            status->clrStatusColor = RGB(255, 0, 0);
            break;
        case STATE_WEB_SOCKET_CREATION_SUCCESSFUL:
            status->nStatusCode = 0x00000000;
            strcpy(status->szLongMessage, "WebSocket connection has been established.");
            strcpy(status->szShortMessage, "OPENED");
            status->clrStatusColor = RGB(0, 255, 0);
            break;
        case STATE_WEB_SOCKET_RECEIVING_RESPONSE:
            status->nStatusCode = 0x00000000;
            strcpy(status->szLongMessage, "Receiving response from the server");
            strcpy(status->szShortMessage, "RECEIVING");
            status->clrStatusColor = RGB(0, 255, 0);
            break;
        case STATE_WEB_SOCKET_RESPONSE_ERROR:
            status->nStatusCode = 0x20000000;
            strcpy(status->szLongMessage, "An error occurred while receiving response from the server");
            strcpy(status->szShortMessage, "ERROR");
            status->clrStatusColor = RGB(255, 0, 0);
            break;
        case STATE_WEB_SOCKET_OUT_OF_MEMORY:
            status->nStatusCode = 0x20000000;
            strcpy(status->szLongMessage, "Out of memory while receiving response from the server. Connection terminated.");
            strcpy(status->szShortMessage, "ERROR");
            status->clrStatusColor = RGB(255, 255, 0);
            break;
        case STATE_WEB_SOCKET_CONNECTION_LOST:
            status->nStatusCode = 0x20000000;
            strcpy(status->szLongMessage, "Connection lost.");
            strcpy(status->szShortMessage, "ERROR");
            status->clrStatusColor = RGB(255, 255, 0);
            break;
        default:
            strcpy(status->szShortMessage, "Unknown");
            strcpy(status->szLongMessage, "UNKNOWN");
            status->clrStatusColor = RGB(255, 255, 255);
            break;
        }
        return 1;
    }
}

PLUGINAPI int GetSymbolLimit(void) {
    return SymbolLimit;
}

PLUGINAPI struct RecentInfo* GetRecentInfo(LPCTSTR pszTicker) {
    return FindOrAddRecentInfo(pszTicker);
}