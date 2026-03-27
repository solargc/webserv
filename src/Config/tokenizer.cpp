#include "Config.hpp"

std::vector<std::string> Config::tokenize(const std::string &content) {
    std::vector<std::string> tokens;
    std::string token;

    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];
        if (c == '{' || c == '}' || c == ';') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back(std::string(1, c));
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else
            token += c;
    }
    return tokens;
}
