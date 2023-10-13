#include "bencode.h"

namespace Bencode {
    Decoder::Decoder(const std::string &torrent_string)
            : torrent_string(torrent_string)
    {}

    element Decoder::DecodeElement() {
        switch(torrent_string[0]) {
            case 'i': {
                auto num = DecodeNumber();
                return element({ .value = num });
            }
            case 'l': {
                auto arr = DecodeList();
                return element({ .value = arr });
            }
            case 'd': {
                auto dict = DecodeDictionary();
                return element({ .value = dict });
            }
            default: {
                auto str = DecodeString();
                return element({ .value = str });
            }
        }
    }

    number Decoder::DecodeNumber() {
        assert(torrent_string[0] == 'i');
        torrent_string.remove_prefix(1);

        number res{};
        auto [ptr, errc] = std::from_chars(torrent_string.cbegin(), torrent_string.cend(), res);

        if (errc == std::errc::invalid_argument)
            std::cerr << "Error occurred while decoding a number: that isn't a number.\n";
        else if (errc == std::errc::result_out_of_range)
            std::cerr << "Error occured while decoding a number: this number is larger than an long long.\n";

        assert(*ptr == 'e');
        torrent_string.remove_prefix(ptr + 1 - torrent_string.cbegin());

        return res;
    }

    string Decoder::DecodeString() {
        assert(std::isdigit(torrent_string[0]));

        size_t length{};
        auto [ptr, errc] = std::from_chars(torrent_string.cbegin(), torrent_string.cend(), length);

        if (errc == std::errc::invalid_argument)
            std::cerr << "Error occurred while decoding a string length: that isn't a number.\n";
        else if (errc == std::errc::result_out_of_range)
            std::cerr << "Error occured while decoding a string length: this number is larger than an size_t.\n";

        torrent_string.remove_prefix(ptr - torrent_string.cbegin());

        assert(*ptr == ':');
        ++ptr;
        torrent_string.remove_prefix(1);

        string res{ptr, length};
        torrent_string.remove_prefix(length);

        return res;
    }

    list Decoder::DecodeList() {
        assert(torrent_string[0] == 'l');
        torrent_string.remove_prefix(1);

        list res;
        while (torrent_string[0] != 'e')
            res.push_back(DecodeElement());

        assert(torrent_string[0] == 'e');
        torrent_string.remove_prefix(1);

        return res;
    }

    dictionary Decoder::DecodeDictionary() {
        assert(torrent_string[0] == 'd');
        torrent_string.remove_prefix(1);

        dictionary res;
        while (torrent_string[0] != 'e') {
            auto key = DecodeString();
            res.insert({key, DecodeElement()});
        }

        assert(torrent_string[0] == 'e');
        torrent_string.remove_prefix(1);

        return res;
    }

    Encoder::Encoder() = default;

    std::string Encoder::EncodeElement(element &element_) {
        if (std::holds_alternative<number>(element_.value))
            return EncodeNumber(std::get<number>(element_.value));
        else if (std::holds_alternative<string>(element_.value))
            return EncodeString(std::get<string>(element_.value));
        else if (std::holds_alternative<list>(element_.value))
            return EncodeList(std::get<list>(element_.value));
        else
            return EncodeDictionary(std::get<dictionary>(element_.value));
    }

    std::string Encoder::EncodeNumber(number &number_) {
        return "i" + std::to_string(number_) + "e";
    }

    std::string Encoder::EncodeString(const string &string_) {
        return std::to_string(string_.length()) + ":" + string_;
    }

    std::string Encoder::EncodeList(list &list_) {
        std::string res = "l";
        for (auto& element_: list_)
            res += EncodeElement(element_);
        return res + "e";
    }

    std::string Encoder::EncodeDictionary(dictionary &dictionary_) {
        std::string res = "d";
        for (auto& [key, val]: dictionary_) {
            res += EncodeString(key);
            res += EncodeElement(val);
        }
        return res + "e";
    }
}
