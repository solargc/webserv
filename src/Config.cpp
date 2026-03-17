#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

Config::Config(const std::string &path) {
  std::ifstream file(path.c_str());
  if (!file.is_open())
    throw std::runtime_error("Cannot open config file: " + path);

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  std::vector<std::string> tokens = tokenize(content);
  for (size_t i = 0; i < tokens.size(); i++)
    std::cout << tokens[i] << std::endl;
}

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

const std::vector<ServerConfig> &Config::getServers() const { return _servers; }
