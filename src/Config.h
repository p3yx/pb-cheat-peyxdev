#pragma once
#include <string>
#include <unordered_map>

// Parser config sederhana: key = value, komentar pakai # atau ;
class Config {
public:
    bool load(const std::string& path);

    bool   has(const std::string& key) const;
    std::string getString(const std::string& key, const std::string& def = "") const;
    int         getInt(const std::string& key, int def = 0) const;
    unsigned long long getHex(const std::string& key, unsigned long long def = 0) const;
    float       getFloat(const std::string& key, float def = 0.0f) const;
    bool        getBool(const std::string& key, bool def = false) const;

    void set(const std::string& key, const std::string& value);
    void setBool(const std::string& key, bool v);
    void setInt(const std::string& key, int v);
    void setFloat(const std::string& key, float v);
    void setHex(const std::string& key, unsigned long long v);
    bool save(const std::string& path) const;

private:
    std::unordered_map<std::string, std::string> m_values;
};
