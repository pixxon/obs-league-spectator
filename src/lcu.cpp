#include "lcu.h"

#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/InvalidCertificateHandler.h>
#include <Poco/Net/ConsoleCertificateHandler.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/StreamCopier.h>
#include <Poco/Path.h>
#include <Poco/URI.h>
#include <Poco/Exception.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/JSONException.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>

#include <fstream>

std::array<player, 10> get_players()
{
    std::array<player, 10> result;

    try
    {
        Poco::URI uri("https://127.0.0.1:2999/liveclientdata/allgamedata");
        Poco::Net::HTTPSClientSession session(uri.getHost(), uri.getPort());

        std::string path(uri.getPathAndQuery());
        Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
        session.sendRequest(req);

        Poco::Net::HTTPResponse res;
        if(res.getStatus() == Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK && res.getContentLength() != 0)
        {
            std::istream &is = session.receiveResponse(res);

            Poco::JSON::Parser parser;
            Poco::Dynamic::Var rawContent = parser.parse(is);
            Poco::JSON::Object::Ptr content = rawContent.extract<Poco::JSON::Object::Ptr>();
            Poco::JSON::Array::Ptr allPlayers = content->getArray("allPlayers");

            if (allPlayers != nullptr)
            {
                for(auto i = 0u; i < std::min<size_t>(allPlayers->size(), 10u); ++i)
                {
                    Poco::JSON::Object::Ptr player = allPlayers->getObject(i);

                    result[i].name = player->getValue<std::string>("summonerName");
                    result[i].champion = player->getValue<std::string>("championName");
                    result[i].team = player->getValue<std::string>("team");
                }
            }
        }
    }
    catch(const Poco::Net::NetException&)
    {
    }
    
    return result;
}

std::map<std::string, std::string> get_pros(const std::string& path)
{
    std::fstream f(path);

    std::map<std::string, std::string> result;

    try
    {
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var content = parser.parse(f);
        Poco::JSON::Array::Ptr allPros = content.extract<Poco::JSON::Array::Ptr>();

        for(auto i = 0u; i < allPros->size(); ++i)
        {
            Poco::JSON::Object::Ptr player = allPros->getObject(i);

            auto summonerName = player->getValue<std::string>("summonerName");
            auto proName = player->getValue<std::string>("proName");

            result.emplace(summonerName, proName);
        }
    }
    catch(const Poco::JSON::JSONException&)
    {

    }

    return result;
}

void init_lcu()
{
    Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> pCert = new Poco::Net::ConsoleCertificateHandler(false); 
    Poco::Net::Context::Ptr pContext = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", Poco::Net::Context::VERIFY_NONE);
    Poco::Net::SSLManager::instance().initializeClient(0, pCert, pContext);
}
