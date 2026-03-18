#include "Config.hpp"

std::vector<std::string> Config::tokenize(const std::string &content) {
    std::vector<std::string> tokens;
    std::string word;

    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];
        if (c == '{' || c == '}' || c == ';') {
            if (!word.empty()) {
                tokens.push_back(word);
                word.clear();
            }
            tokens.push_back(std::string(1, c));
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!word.empty()) {
                tokens.push_back(word);
                word.clear();
            }
        } else
            word += c;
    }
    return tokens;
}
