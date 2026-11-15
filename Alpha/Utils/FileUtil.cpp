#include "FileUtil.h"

std::string FileUtil::getRoamingStatePath() {
	char* localAppDataPath = nullptr;
	size_t len = 0;
	if (_dupenv_s(&localAppDataPath, &len, "LOCALAPPDATA") == 0 && localAppDataPath != nullptr) {
		std::filesystem::path roamingState = std::filesystem::path(localAppDataPath) /
			"Packages" / "Microsoft.MinecraftUWP_8wekyb3d8bbwe" / "RoamingState";
		free(localAppDataPath);
		return roamingState.string() + "\\";
	}
	return "";
}

std::string FileUtil::getClientPath() {
	static const std::string clientPath = getRoamingStatePath() + "Alpha\\";
	return clientPath;
}

