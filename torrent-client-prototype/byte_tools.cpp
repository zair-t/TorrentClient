#include "byte_tools.h"
#include <openssl/sha.h>
#include <vector>

int BytesToInt(std::string_view bytes) {
    return static_cast<int>(static_cast<unsigned char>(bytes[0]) << 24 |
                            static_cast<unsigned char>(bytes[1]) << 16 |
                            static_cast<unsigned char>(bytes[2]) << 8 |
                            static_cast<unsigned char>(bytes[3]));
}

std::string IntToBytes(uint32_t num, bool isHost) {
    std::string res;
    uint8_t out[4];
    *(uint32_t*)&out = num;
    if (!isHost)
        std::reverse(out, out + 4);
    for (auto &i: out)
        res += (char) i;
    return res;
}

std::string CalculateSHA1(const std::string& msg) {
    unsigned char buffer[20];
    SHA1((unsigned char*)msg.c_str(), msg.length(), buffer);
    std::string hash;
    for (auto &c: buffer)
        hash += char(c);

    return hash;
}

std::string HexEncode(const std::string& input) {
    return ""; 
}