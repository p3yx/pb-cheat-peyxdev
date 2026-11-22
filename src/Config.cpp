#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool Config::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        // buang komentar inline
        auto c = val.find_first_of("#;");
        if (c != std::string::npos) val = trim(val.substr(0, c));
        if (!key.empty()) m_values[key] = val;
    }
    return true;
}

bool Config::has(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

std::string Config::getString(const std::string& key, const std::string& def) const {
    auto it = m_values.find(key);
    return it == m_values.end() ? def : it->second;
}

int Config::getInt(const std::string& key, int def) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    try { return std::stoi(it->second, nullptr, 0); } catch (...) { return def; }
}

unsigned long long Config::getHex(const std::string& key, unsigned long long def) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    try { return std::stoull(it->second, nullptr, 0); } catch (...) { return def; }
}

float Config::getFloat(const std::string& key, float def) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    try { return std::stof(it->second); } catch (...) { return def; }
}

bool Config::getBool(const std::string& key, bool def) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return def;
    auto v = it->second;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return def;
}

void Config::set(const std::string& key, const std::string& value) { m_values[key] = value; }
void Config::setBool(const std::string& key, bool v) { set(key, v ? "1" : "0"); }
void Config::setInt(const std::string& key, int v) { set(key, std::to_string(v)); }
void Config::setFloat(const std::string& key, float v) { set(key, std::to_string(v)); }
void Config::setHex(const std::string& key, unsigned long long v) {
    std::ostringstream ss; ss << "0x" << std::hex << v; set(key, ss.str());
}

bool Config::save(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return false;
    // tulis ulang nilai yang ada (format key=value, komentar tidak dipertahankan)
    for (const auto& kv : m_values) f << kv.first << " = " << kv.second << "\n";
    return true;
}
