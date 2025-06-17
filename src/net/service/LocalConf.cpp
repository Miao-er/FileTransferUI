#include <iostream>
#include <fstream>
#include <sstream>
#include "LocalConf.h"

bool LocalConf::isCommentOrEmpty(const std::string& line) const{
    return line.empty() || line[0] == '#';
}

std::string& LocalConf::trim(std::string& str) {
    str.erase(0, str.find_first_not_of(" \t")); 
    str.erase(str.find_last_not_of(" \t") + 1);
    return str;
}

int LocalConf::splitComma(const std::string& splitString, std::vector<std::string>& splitArray) {
    std::stringstream ss(splitString);
    std::string item;
    int num = 0;
    while (std::getline(ss, item, ',')){
        splitArray.push_back(trim(item)); // 去除每个IP网段的空格
        num++;
    }
    return num;
}

bool LocalConf::safeStringToInt(const std::string& str, int& result, const std::string& fieldName) {
    try {
        result = std::stoi(str);
        return true;
    } catch (const std::invalid_argument& e) {
        std::cout << "[Error] Invalid " << fieldName << " value: " << str << " - not a valid number" << std::endl;
        return false;
    } catch (const std::out_of_range& e) {
        std::cout << "[Error] " << fieldName << " value out of range: " << str << std::endl;
        return false;
    }
}

bool LocalConf::safeStringToDouble(const std::string& str, double& result, const std::string& fieldName)
{
    try {
        result = std::stod(str);
        return true;
    }
    catch (const std::invalid_argument& e) {
        std::cout << "[Error] Invalid " << fieldName << " value: " << str << " - not a valid number" << std::endl;
        return false;
    }catch(const std::out_of_range& e){
        std::cout << "[Error] " << fieldName << " value out of range: " << str << std::endl;
        return false;
    }
}

bool LocalConf::safeStringToULongLong(const std::string& str, unsigned long long& result, const std::string& fieldName) {
    try {
        result = std::stoull(str);
        return true;
    } catch (const std::invalid_argument& e) {
        std::cout << "[Error] Invalid " << fieldName << " value: " << str << " - not a valid number" << std::endl;
        return false;
    } catch (const std::out_of_range& e) {
        std::cout << "[Error] " << fieldName << " value out of range: " << str << std::endl;
        return false;
    }
}

int LocalConf::createDefaultConf() {
    int ret = saveConf();
    if(ret == 0)
        std::cout << "Create default config file: " << this->configPath << std::endl;
    return ret;
}

int LocalConf::saveConf() {
    std::ofstream file(this->configPath);
    if (!file.is_open()) {
        std::cout << "Failed to create config file: " << this->configPath << std::endl;
        return -1;
    }

    // 写入默认配置
    file << "# Configuration File\n"
         << "RdmaGidIndex = " << this->rdmaGidIndex << "\n"
         << "ListenPort = " << this->localPort << "\n"
         << "MaxThreadNum = " << this->maxThreadNum << "\n"
         << "DefaultRate = " << this->defaultRate << "\n"
         << "BlockSize = " << this->blockSize << "\n"
         << "BlockNum = " << this->blockNum << "\n";

    file.close();
    return 0;
}

int LocalConf::loadConf() {
    std::ifstream file(this->configPath);

    if (!file.is_open()) {
        createDefaultConf();
        if (!file.is_open()) {
            std::cout << "Failed to open configuration file: " << this->configPath << std::endl;
                return -1;
        }
    }
    
    std::cout << "loading config \"" <<this->configPath << "\"..." << std::endl;
    std::string line;
    bool inInterfaceSection = false;
    bool inPeerSection = false;
    
    while (std::getline(file, line)) {
        // 去除行首尾空格
        trim(line);

        if (isCommentOrEmpty(line)) {
            continue; // 跳过注释和空行
        }

        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            std::cout << "[Error] Invalid config line: " << line << std::endl;
                return -1;
            }

        std::string key = line.substr(0, pos);
        trim(key);
        std::string value = line.substr(pos + 1);
        trim(value);

        if (key == "RdmaGidIndex") {
            if (!safeStringToInt(value, this->rdmaGidIndex, "RdmaGidIndex")) {
                return -1;
            }
            if(this->rdmaGidIndex < 0)
            {
                std::cout << "[Error] Invalid RdmaGidIndex: " << value << std::endl;
                return -1;
            }
        }
        else if (key == "ListenPort")
        {
            if (!safeStringToInt(value, this->localPort, "ListenPort")) {
                return -1;
            }
            if(this->localPort <= 0 || this->localPort > 65535)
            {
                std::cout << "[Error] Invalid ListenPort: " << value << std::endl;
                return -1;
            }
        }
        else if (key == "MaxThreadNum")
        {
            if (!safeStringToInt(value, this->maxThreadNum, "MaxThreadNum")) {
                return -1;
            }
            if(this->maxThreadNum <= 0 || this->maxThreadNum > 1024)
            {
                std::cout << "[Error] Invalid MaxThreadNum: " << value << std::endl;
                std::cout << "Valid range: 1 ~ 1024" << std::endl;
                return -1;
            }
        }
        else if (key == "DefaultRate")
        {
            if (!safeStringToDouble(value, this->defaultRate, "DefaultRate")) {
                return -1;
            }
            if(this->defaultRate <= 0)
            {
                std::cout << "[Error] Invalid DefaultRate: " << value << std::endl;
                return -1;
            }
        }
        else if (key == "BlockSize")
        {
            if (!safeStringToInt(value, this->blockSize, "BlockSize")) {
                return -1;
            }
            if(this->blockSize <= 4 || this->blockSize > 1024 * 1024)  //4k ~ 1G
            {
                std::cout << "[Error] Invalid BlockSize: " << value << std::endl;
                std::cout << "Valid range: 4 ~ 1048576" << std::endl;
                return -1; // 修复：添加缺失的return
            }
        }
        else if (key == "BlockNum")
        {
            if (!safeStringToInt(value, this->blockNum, "BlockNum")) {
                return -1;
            }
            if(this->blockNum <= 0 || this->blockNum > 65536) // 1 ~ 65536
            {
                std::cout << "[Error] Invalid BlockNum: " << value << std::endl;
                std::cout << "Valid range: 1 ~ 65536" << std::endl;
                return -1;
            }
        }
        else
        {
            std::cout << "[Error] When parsing config_file, unknown key: " << key << std::endl;
            return -1;
        }
    }
    
    file.close();
    std::cout << "Configuration loaded successfully from: " << this->configPath << std::endl;
    return 0; 
}