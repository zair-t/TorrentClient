#include <sstream>
#include "message.h"

Message Message::Parse(const std::string &messageString) {
    if (messageString.empty())
        return Init(MessageId::KeepAlive, "");
    auto messageId = (MessageId) messageString[0];
    std::string payload = messageString.substr(1);
    return Init(messageId, payload);
}

Message Message::Init(MessageId id, const std::string &payload) {
    return Message({.id = id, .messageLength = payload.length() + 1, .payload = payload});
}

std::string Message::ToString() const {
    std::stringstream buf;
    char* msgLen = (char*) &messageLength;
    for (int i = 0; i < 4; ++i)
        buf << (char) msgLen[3-i];
    buf << (char) id;
    buf << payload;
    return buf.str();
}