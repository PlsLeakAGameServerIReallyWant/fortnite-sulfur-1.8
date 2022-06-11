#pragma once

#include "SDK.hpp"

#include "minhook/MinHook.h"
#include "NetHooks.h"

#pragma comment(lib, "minhook/minhook.lib")

using namespace SDK;

namespace Hooks
{
	bool bIsReady = false;
	bool bHasSpawned = false;
	bool bIsInGame = false;
	bool bHasInitedTheBeacon = false;

	LPVOID(*ProcessEvent)(void*, void*, void*);
	LPVOID ProcessEventHook(UObject* pObject, UFunction* pFunction, LPVOID pParams)
	{
		if (pFunction->GetName().find("BP_PlayButton") != std::string::npos)
		{
			Globals::PC->SwitchLevel(TEXT("Athena_Faceoff?Game=Athena"));

			NetHooks::BeaconHost = new Beacon();
			NetHooks::NetReplicator = new Replicator(NetHooks::BeaconHost);

			auto BaseAddr = Util::BaseAddress();
			auto AddNetworkActorAddr = BaseAddr + 0x25108F0;

			NetHooks::AddNetworkActor = decltype(NetHooks::AddNetworkActor)(AddNetworkActorAddr);

			MH_CreateHook((void*)(AddNetworkActorAddr), NetHooks::AddNetworkActorHook, (void**)(&NetHooks::AddNetworkActor));
			MH_EnableHook((void*)(AddNetworkActorAddr));

			bIsReady = true;
		}
		
		if (pFunction->GetName().find("Tick") != std::string::npos)
		{
			if (GetAsyncKeyState(VK_F1) & 0x1)
			{
				if (!bHasInitedTheBeacon) {
					Beacon::InitOffsets();

					NetHooks::Init();
					bHasInitedTheBeacon = true;
				}
			}

			if (GetAsyncKeyState(VK_F2) & 0x1)
			{
				auto Location = Globals::PC->Pawn->K2_GetActorLocation();
				auto NewFortPickup = reinterpret_cast<AFortPickup*>(Util::SpawnActor(AFortPickup::StaticClass(), Location, FRotator()));

				NewFortPickup->PrimaryPickupItemEntry.Count = 1;
				NewFortPickup->PrimaryPickupItemEntry.ItemDefinition = UObject::FindObject<UFortWeaponItemDefinition>("WID_Assault_AutoHigh_Athena_SR_Ore_T03.WID_Assault_AutoHigh_Athena_SR_Ore_T03");
				NewFortPickup->OnRep_PrimaryPickupItemEntry();

				NewFortPickup->TossPickup(Location, nullptr, 1, true);
			}

			if (GetAsyncKeyState(VK_F3) & 0x1)
			{
				auto Pawn = (APlayerPawn_Athena_C*)Util::SpawnActor(APlayerPawn_Athena_C::StaticClass(), { 0,0,500 }, { 0,0,0 });
				Globals::PC->Possess(Pawn);

				((AFortPlayerController*)(Globals::PC))->ServerReadyToStartMatch();
				((AGameMode*)(Globals::World->AuthorityGameMode))->StartMatch();
			}
		}

		if (pFunction->GetName().find("ReadyToStartMatch") != std::string::npos && bIsReady)
		{
			Globals::FortEngine = UObject::FindObject<UFortEngine>("FortEngine_");
			Globals::World = Globals::FortEngine->GameViewport->World;
			Globals::PC = reinterpret_cast<AFortPlayerController*>(Globals::FortEngine->GameInstance->LocalPlayers[0]->PlayerController);
		}

		return ProcessEvent(pObject, pFunction, pParams);
	}

	static void Init()
	{
		auto FEVFT = *reinterpret_cast<void***>(Globals::FortEngine);
		auto PEAddr = FEVFT[62];

		MH_CreateHook(reinterpret_cast<LPVOID>(PEAddr), ProcessEventHook, reinterpret_cast<LPVOID*>(&ProcessEvent));
		MH_EnableHook(reinterpret_cast<LPVOID>(PEAddr));
	}
}