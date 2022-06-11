#pragma once

using namespace SDK;

namespace NetHooks
{
	Beacon* BeaconHost;
	Replicator* NetReplicator;

	void(*UWorld_NotifyControlMessage)(UWorld* World, UNetConnection* NetConnection, uint8_t a3, void* a4);
	__int64(*WelcomePlayer)(UWorld* This, UNetConnection* NetConnection);
	APlayerController* (*SpawnPlayActor)(UWorld* a1, UPlayer* a2, ENetRole a3, FURL a4, void* a5, FString& Src, uint8_t a7);
	void (*TickFlush)(UNetDriver*, float DeltaSeconds);
	void (*AddNetworkActor)(UWorld*, AActor*);

	void AddNetworkActorHook(UWorld* World, AActor* Actor)
	{
		//SULFUR_LOG("AddNetworkActor Called, adding " << Actor->GetName() << " to actors to replicate!");

		FNetworkObjectInfo* NewActorObjectInfo = new FNetworkObjectInfo();
		NewActorObjectInfo->Actor = Actor;

		NetReplicator->PrivateObjectList.push_back(NewActorObjectInfo);

		return AddNetworkActor(World, Actor);
	}

	void TickFlushHook(UNetDriver* NetDriver, float DeltaSeconds)
	{
		//printf("TickFlush called!\n");

		auto Result = NetReplicator->ServerReplicateActors(DeltaSeconds);
		//SULFUR_LOG("Replicated " << Result << " Actors!")

		return TickFlush(NetDriver, DeltaSeconds);
	}

	void __fastcall AOnlineBeaconHost_NotifyControlMessageHook(AOnlineBeaconHost* BeaconHost, UNetConnection* NetConnection, uint8_t a3, void* a4)
	{
		if (TO_STRING(a3) == "4") {
			NetConnection->CurrentNetSpeed = 30000;
			return;
		}

		SULFUR_LOG("AOnlineBeaconHost::NotifyControlMessage Called! " << TO_STRING(a3).c_str());
		SULFUR_LOG("Channel Count " << TO_STRING(NetConnection->OpenChannels.Num()).c_str());
		return UWorld_NotifyControlMessage(Globals::World, NetConnection, a3, a4);
	}

	__int64 __fastcall WelcomePlayerHook(UWorld*, UNetConnection* NetConnection)
	{
		SULFUR_LOG("Welcoming Player!");
		return WelcomePlayer(Globals::World, NetConnection);
	}

	APlayerController* SpawnPlayActorHook(UWorld*, UNetConnection* Connection, ENetRole NetRole, FURL a4, void* a5, FString& Src, uint8_t a7)
	{
        auto PlayerController = SpawnPlayActor(Globals::World, Connection, NetRole, a4, a5, Src, a7);
		Connection->PlayerController = PlayerController;
		PlayerController->NetConnection = Connection;
		Connection->OwningActor = PlayerController;

        return PlayerController;
	}

	char KickPlayerHook()
	{
		return 0;
	}

	static void Init()
	{
		auto BaseAddr = Util::BaseAddress();

		UWorld_NotifyControlMessage = decltype(UWorld_NotifyControlMessage)(BaseAddr + UWORLD_NCM_OFFSET);
	
		auto AOnlineBeaconHost_NotifyControlMessageAddr = BaseAddr + AONLINEBEACONHOST_NCM_OFFSET;
		auto WelcomePlayerAddr = BaseAddr + WELCOME_PLAYER_OFFSET;
		auto SpawnPlayActorAddr = BaseAddr + SPAWN_PLAY_ACTOR_OFFSET;
		auto TickFlushAddr = BaseAddr + 0x22B2F50;
		//auto KickPlayerAddr = Util::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 49 8B F0 48 8B DA 48 85 D2 74 ? 48 8B BA ? ? ? ? 48");

		TickFlush = decltype(TickFlush)(TickFlushAddr);
		SpawnPlayActor = decltype(SpawnPlayActor)(SpawnPlayActorAddr);

		MH_CreateHook((void*)(AOnlineBeaconHost_NotifyControlMessageAddr), AOnlineBeaconHost_NotifyControlMessageHook, nullptr);
		MH_EnableHook((void*)(AOnlineBeaconHost_NotifyControlMessageAddr));
		MH_CreateHook((void*)(WelcomePlayerAddr), WelcomePlayerHook, (void**)(&WelcomePlayer));
		MH_EnableHook((void*)(WelcomePlayerAddr));
		MH_CreateHook((void*)(SpawnPlayActorAddr), SpawnPlayActorHook, (void**)(&SpawnPlayActor));
		MH_EnableHook((void*)(SpawnPlayActorAddr));
		//MH_CreateHook((void*)(KickPlayerAddr), KickPlayerHook, nullptr);
		//MH_EnableHook((void*)(KickPlayerAddr));


		BeaconHost->Spawn(7777);

		MH_CreateHook((void*)(TickFlushAddr), TickFlushHook, (void**)(&TickFlush));
		MH_EnableHook((void*)(TickFlushAddr));
	}
}
