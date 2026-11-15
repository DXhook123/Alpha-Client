#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace FileUtil {

	std::string getRoamingStatePath();
	std::string getClientPath();

	inline bool doesFilePathExist(const std::string& path) {
		return std::filesystem::exists(path);
	}

	inline bool createPath(const std::string& path) {
		return std::filesystem::create_directories(path);
	}

	inline bool deletePath(const std::string& path) {
		return std::filesystem::remove(path);
	}

	inline bool writeBinary(const std::string& path, const std::string& data) {
		std::ofstream ofs(path, std::ios::binary);
		if (!ofs.is_open()) return false;
		ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
		return ofs.good();
	}

	inline bool writeFile(const std::string& path, const std::string& data) {
		std::ofstream ofs(path);
		if (!ofs.is_open()) return false;
		ofs << data;
		return ofs.good();
	}

	inline std::string readFile(const std::string& path) {
		std::ifstream ifs(path);
		if (!ifs.is_open()) return "";
		std::stringstream ss;
		ss << ifs.rdbuf();
		return ss.str();
	}
}
