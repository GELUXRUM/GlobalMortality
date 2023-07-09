#include "SimpleIni.h"
#include <cctype>

CSimpleIniA ini(true, false, false);
bool makeProtected;

bool GetOptionValue(std::string a_optionName, const char* a_optionValue)
{
	std::string lowercaseStr;
	size_t length = std::strlen(a_optionValue);
	for (size_t i = 0; i < length; ++i) {
		lowercaseStr += static_cast<char>(std::tolower(static_cast<unsigned char>(a_optionValue[i])));
	}

	if (lowercaseStr == "true") {
		return true;
	} else if (lowercaseStr == "false") {
		return false;
	} else {
		logger::warn("Invalid value passed to {}. Defaulting to false", a_optionName);
		return false;
	}
}

void LoadConfigs()
{
	ini.LoadFile("Data\\F4SE\\Plugins\\GLXRM_GlobalMortality.ini");

	auto protectedOptionValue = ini.GetValue("General", "MakeProtected", "false");
	makeProtected = GetOptionValue("MakeProtected", protectedOptionValue);

	ini.Reset();
}

void GameDataListener(F4SE::MessagingInterface::Message* thing)
{
	std::vector<RE::TESActorBase*> abArray;

	if (thing->type == F4SE::MessagingInterface::kGameDataReady) {
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
			for (auto currentForm : dataHandler->GetFormArray<RE::TESNPC>()) {
				if (currentForm->IsEssential()) {
					abArray.push_back(currentForm);
				}
			}

			for (auto currentActorBase : abArray) {
				currentActorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
				if (makeProtected) {
					currentActorBase->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kProtected);
				}
			}
			
			if (makeProtected) {
				logger::warn("{} NPCs have had their Essential flag replaced with the Protected flag", abArray.size());
			} else {
				logger::warn("{} NPCs have had their Essential flag removed", abArray.size());
			}
			
		}
	}

	return;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "GLXRM_GlobalMortality";
	a_info->version = 1;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical(FMT_STRING("unsupported runtime v{}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	LoadConfigs();

	F4SE::GetMessagingInterface()->RegisterListener(GameDataListener);

	return true;
}
