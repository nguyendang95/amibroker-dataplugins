#include "Binance.h"
#include "nlohmann/json.hpp"
#include "AmibrokerUtilities.h"
#include "Plugin.h"

using namespace nlohmann;

std::string SubscribeRequest::ToJson() {
    nlohmann::json json;
    json["id"] = Id;
    json["method"] = Method.c_str();
    json["params"] = Params;
    return std::string(json.dump());
}

SubscribeRequest::SubscribeRequest(std::vector<std::string>params, std::string method, int id) {
    Params = params;
    Method = method;
    Id = id;
}

BinanceStreamDataDetail ParseBinanceStreamDataDetail(const json json) {
    BinanceStreamDataDetail data;
    long long tmp;
    std::string tmp2;
    if (!json.at("t").is_null()) {
        tmp = (long long)json["t"] / 1000;
        data.KlineStartTime = EpochTimeToLocalTime(tmp);
    }
    if (!json.at("T").is_null()) {
        tmp = (long long)json["T"] / 1000;
        data.KlineCloseTime = EpochTimeToLocalTime(tmp);
    }
    if (!json.at("s").is_null()) json.at("s").get_to<std::string>(data.Symbol);
    if (!json.at("i").is_null()) json.at("i").get_to<std::string>(data.Interval);
    if (!json.at("f").is_null()) json.at("f").get_to<unsigned int>(data.FirstTradeId);
    if (!json.at("L").is_null()) json.at("L").get_to<unsigned int>(data.LastTradeId);
    if (!json.at("o").is_null()) {
        json.at("o").get_to<std::string>(tmp2);
        data.OpenPrice = std::stof(tmp2);
    }
    if (!json.at("c").is_null()) {
        json.at("c").get_to<std::string>(tmp2);
        data.ClosePrice = std::stof(tmp2);
    }
    if (!json.at("h").is_null()) {
        json.at("h").get_to<std::string>(tmp2);
        data.HighPrice = std::stof(tmp2);
    }
    if (!json.at("l").is_null()) {
        json.at("l").get_to<std::string>(tmp2);
        data.LowPrice = std::stof(tmp2);
    }
    if (!json.at("v").is_null()) {
        json.at("v").get_to<std::string>(tmp2);
        data.BaseAssetVolume = std::stof(tmp2);
    }
    if (!json.at("n").is_null()) json.at("n").get_to<unsigned int>(data.NumberOfTrades);
    if (!json.at("x").is_null()) json.at("x").get_to<bool>(data.IsThisLineClosed);
    if (!json.at("q").is_null()) {
        json.at("q").get_to<std::string>(tmp2);
        data.QuoteAssetVolume = std::stof(tmp2);
    }
    if (!json.at("V").is_null()) {
        json.at("V").get_to<std::string>(tmp2);
        data.TakerBuyBaseAssetVolume = std::stof(tmp2);
    }
    if (!json.at("Q").is_null()) {
        json.at("Q").get_to<std::string>(tmp2);
        data.TakerBuyQuoteAssetVolume = std::stof(tmp2);
    }
    if (!json.at("B").is_null()) {
        json.at("B").get_to<std::string>(tmp2);
        data.Ignore = std::stof(tmp2);
    }
    return data;
}

BinanceStreamData ParseBinanceStreamData(const json json) {
    BinanceStreamData response;
    if (json.contains("e")) {
        if (!json.at("e").is_null()) {
            long long tmp;
            if (!json.at("e").is_null()) json.at("e").get_to<std::string>(response.EventType);
            if (!json.at("E").is_null()) {
                tmp = (long long)json["E"] / 1000;
                response.EventTime = EpochTimeToLocalTime(tmp);
            }
            if (!json.at("s").is_null()) json.at("s").get_to<std::string>(response.Symbol);
            if (!json.at("k").is_null()) response.Data = ParseBinanceStreamDataDetail(json["k"]);
        }
    }
    return response;
}

BinanceStreamResponse ParseBinanceStreamResponse(const json json) {
    BinanceStreamResponse response;
    if (json.contains("stream")) {
        if (!json.at("stream").is_null()) json.at("stream").get_to<std::string>(response.Stream);
        if (!json.at("data").is_null()) response.Data = ParseBinanceStreamData(json["data"]);
    }
    return response;
}