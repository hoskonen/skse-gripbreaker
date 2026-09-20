#include "CombatDiagnostics.h"
#include "PrecisionDiagnostics.h"
#include "Settings.h"
#include "UI.h"

namespace
{
	void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (a_message->type == SKSE::MessagingInterface::kPostLoad) {
			UI::Register();
		} else if (a_message->type == SKSE::MessagingInterface::kPostPostLoad) {
			PrecisionDiagnostics::Register();
		} else if (a_message->type == SKSE::MessagingInterface::kNewGame ||
		           a_message->type == SKSE::MessagingInterface::kPostLoadGame) {
			SKSE::GetTaskInterface()->AddTask(CombatDiagnostics::Register);
		}
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	Settings::Load();
	SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);

	logs::info("GripBreaker initialized");

	return true;
}
