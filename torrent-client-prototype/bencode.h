#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <variant>
#include <list>
#include <map>
#include <sstream>
#include <cassert>
#include <charconv>
#include <iostream>

namespace Bencode {

/*
 * В это пространство имен рекомендуется вынести функции для работы с данными в формате bencode.
 * Этот формат используется в .torrent файлах и в протоколе общения с трекером
 */
    struct element;

    using number = long long;
    using string = std::string;
    using list = std::list<element>;
    using dictionary = std::map<string, element>;

    struct element {
        std::variant<number, string, list, dictionary> value;
    };

    class Decoder {
    public:
        explicit Decoder(const std::string& torrent_string);

        element DecodeElement();
    private:
        number DecodeNumber();

        string DecodeString();

        list DecodeList();

        dictionary DecodeDictionary();

        std::string_view torrent_string;
    };

    class Encoder {
    public:
        explicit Encoder();

        std::string EncodeElement(element& element_);
    private:
        static std::string EncodeNumber(number& number_);

        static std::string EncodeString(const string& string_);

        std::string EncodeList(list& list_);

        std::string EncodeDictionary(dictionary& dictionary_);
    };

}
