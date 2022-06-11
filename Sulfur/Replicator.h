#pragma once

bool bReadyToReplicate = false;

#define CLOSEPROXIMITY					500.f
#define NEARSIGHTTHRESHOLD				2000.f
#define MEDSIGHTTHRESHOLD				3162.f
#define FARSIGHTTHRESHOLD				8000.f
#define CLOSEPROXIMITYSQUARED			(CLOSEPROXIMITY*CLOSEPROXIMITY)
#define NEARSIGHTTHRESHOLDSQUARED		(NEARSIGHTTHRESHOLD*NEARSIGHTTHRESHOLD)
#define MEDSIGHTTHRESHOLDSQUARED		(MEDSIGHTTHRESHOLD*MEDSIGHTTHRESHOLD)
#define FARSIGHTTHRESHOLDSQUARED		(FARSIGHTTHRESHOLD*FARSIGHTTHRESHOLD)

int32_t GSRandSeed;

template< class T >
static inline T Clamp(const T X, const T Min, const T Max)
{
	return X < Min ? Min : X < Max ? X : Max;
};

inline int32_t Rand() { return rand(); };
inline float FRand() { return Rand() / (float)RAND_MAX; };

template< class T >
inline T Max(const T A, const T B)
{
	return (A >= B) ? A : B;
};

template< class T >
inline T Min(const T A, const T B)
{
	return (A <= B) ? A : B;
};

int32_t TruncToInt(float F)
{
	return (int32_t)F;
};

float TruncToFloat(float F)
{
	return (float)TruncToInt(F);
};

float Fractional(float Value)
{
	return Value - TruncToFloat(Value);
};

float SRand()
{
	GSRandSeed = (GSRandSeed * 196314165) + 907633515;
	union { float f; int32_t i; } Result;
	union { float f; int32_t i; } Temp;
	const float SRandTemp = 1.0f;
	Temp.f = SRandTemp;
	Result.i = (Temp.i & 0xff800000) | (GSRandSeed & 0x007fffff);
	return Fractional(Result.f);
};

int Square(const int A)
{
	return A * A;
};

int32_t FloorToInt(float F)
{
	return TruncToInt(floorf(F));
};

int32_t RoundToInt(float F)
{
	return FloorToInt(F + 0.5f);
};

struct FNetworkObjectInfo
{
public:
	/** Pointer to the replicated actor. */
	AActor* Actor;

	/** Next time to consider replicating the actor. Based on FPlatformTime::Seconds(). */
	double NextUpdateTime;

	/** Last absolute time in seconds since actor actually sent something during replication */
	double LastNetReplicateTime;

	/** Optimal delta between replication updates based on how frequently actor properties are actually changing */
	float OptimalNetUpdateDelta;

	/** Last time this actor was updated for replication via NextUpdateTime
	* @warning: internal net driver time, not related to WorldSettings.TimeSeconds */
	float LastNetUpdateTime;

	/** Is this object still pending a full net update due to clients that weren't able to replicate the actor at the time of LastNetUpdateTime */
	uint32_t bPendingNetUpdate : 1;

	/** Force this object to be considered relevant for at least one update */
	uint32_t bForceRelevantNextUpdate : 1;

	/** List of connections that this actor is dormant on */
	std::vector<UNetConnection*> DormantConnections;

	/** A list of connections that this actor has recently been dormant on, but the actor doesn't have a channel open yet.
	*  These need to be differentiated from actors that the client doesn't know about, but there's no explicit list for just those actors.
	*  (this list will be very transient, with connections being moved off the DormantConnections list, onto this list, and then off once the actor has a channel again)
	*/
	std::vector<UNetConnection*> RecentlyDormantConnections;
};

class FNetworkGUID
{
public:

	uint32_t Value;


	FNetworkGUID()
		: Value(0)
	{ }

	FNetworkGUID(uint32_t V)
		: Value(V)
	{ }


	friend bool operator==(const FNetworkGUID& X, const FNetworkGUID& Y)
	{
		return (X.Value == Y.Value);
	}

	friend bool operator!=(const FNetworkGUID& X, const FNetworkGUID& Y)
	{
		return (X.Value != Y.Value);
	}

	static FNetworkGUID Make(int32_t seed, bool bIsStatic)
	{
		return FNetworkGUID(seed << 1 | (bIsStatic ? 1 : 0));
	}
};

struct FActorDestructionInfo
{
	TWeakObjectPtr<ULevel>		Level;
	TWeakObjectPtr<UObject>		ObjOuter;
	FVector			DestroyedPosition;
	FNetworkGUID	NetGUID;
	FString			PathName;

	FName			StreamingLevelName;
};

class Replicator
{
private:
	Beacon* BeaconHost;

public:

	inline static UChannel* (*CreateChannel)(UNetConnection*, int, bool, int32_t);
	inline static __int64 (*ReplicateActorInternal)(UActorChannel*);
	inline static __int64 (*SetChannelActor)(UActorChannel*, AActor*);
	inline static void (*CallPreReplication)(AActor*, UNetDriver*);
	//inline static float (*GetMaxTickRate)(UGameEngine*, float, bool);
	//inline static void (*ActorChannelClose)(UActorChannel*);
	//inline static int64_t (*SetChannelActorForDestroy)(UActorChannel*, FActorDestructionInfo*);

	Replicator(Beacon* InBeaconHost)
	{
		auto BaseAddress = (uintptr_t)GetModuleHandle(NULL);

		auto FEVFT = *reinterpret_cast<void***>(Globals::FortEngine);

		BeaconHost = InBeaconHost;
		Replicator::CreateChannel = decltype(Replicator::CreateChannel)(BaseAddress + 0x22998B0);
		Replicator::SetChannelActor = decltype(Replicator::SetChannelActor)(BaseAddress + 0x2136250);
		Replicator::ReplicateActorInternal = decltype(Replicator::ReplicateActorInternal)(BaseAddress + 0x2131930);
		Replicator::CallPreReplication = decltype(Replicator::CallPreReplication)(BaseAddress + 0x1F2D180);
		//Replicator::GetMaxTickRate = decltype(Replicator::GetMaxTickRate)(FEVFT[79]);
		//Replicator::ActorChannelClose = decltype(Replicator::ActorChannelClose)(BaseAddress + 0x21A8590);
		//Replicator::SetChannelActorForDestroy = decltype(Replicator::SetChannelActorForDestroy)(BaseAddress + 0x21C6A60);
	}

	UActorChannel* FindChannel(AActor* Actor, UNetConnection* Connection)
	{
		for (int i = 0; i < Connection->OpenChannels.Num(); i++)
		{
			auto Channel = Connection->OpenChannels[i];

			if (Channel && Channel->Class)
			{
				if (Channel->Class == UActorChannel::StaticClass())
				{
					if (((UActorChannel*)Channel)->Actor == Actor)
						return ((UActorChannel*)Channel);
				}
			}
		}

		return NULL;
	}

	int32_t ServerReplicateActors_PrepConnections(const float DeltaSeconds)
	{
		int32_t NumClientsToTick = BeaconHost->GetNetDriver()->ClientConnections.Num();

		bool bFoundReadyConnection = false;

		for (int32_t ConnIdx = 0; ConnIdx < BeaconHost->GetNetDriver()->ClientConnections.Num(); ConnIdx++)
		{
			UNetConnection* Connection = BeaconHost->GetNetDriver()->ClientConnections[ConnIdx];

			if (!Connection) continue;

			AActor* OwningActor = Connection->OwningActor;

			if (OwningActor != NULL && (Connection->Driver->Time - Connection->LastReceiveTime < 1.5f))
			{
				bFoundReadyConnection = true;

				AActor* DesiredViewTarget = OwningActor;

				if (Connection->PlayerController)
				{
					if (AActor* ViewTarget = Connection->PlayerController->GetViewTarget())
					{
						if (ViewTarget)
						{
							DesiredViewTarget = ViewTarget;
						}
						else {

						}
					}
				}

				Connection->ViewTarget = DesiredViewTarget;

				for (int32_t ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
				{
					UNetConnection* Child = Connection->Children[ChildIdx];
					APlayerController* ChildPlayerController = Child->PlayerController;
					if (ChildPlayerController != NULL)
					{
						Child->ViewTarget = ChildPlayerController->GetViewTarget();
					}
					else
					{
						Child->ViewTarget = NULL;
					}
				}
			}
			else
			{
				Connection->ViewTarget = NULL;
				for (int32_t ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
				{
					Connection->Children[ChildIdx]->ViewTarget = NULL;
				}
			}
		}

		return bFoundReadyConnection ? NumClientsToTick : 0;
	}

	std::vector<FNetworkObjectInfo*> PrivateObjectList;

	std::vector<FNetworkObjectInfo*>& GetNetworkObjectList()
	{
		return PrivateObjectList;
	}

	void ServerReplicateActors_BuildConsiderList(UNetDriver* NetDriver, std::vector<FNetworkObjectInfo*>& OutConsiderList, float ServerTickTime)
	{
		auto World = NetDriver->World;

		if (!World)
			return;

		static UKismetMathLibrary* MathLib = (UKismetMathLibrary*)UKismetMathLibrary::StaticClass();

		auto TimeSeconds = Globals::GPS->STATIC_GetTimeSeconds(World);

		for (auto ActorInfo : GetNetworkObjectList())
		{
			if (!ActorInfo)
			{
				continue;
			}

			if (!ActorInfo->bPendingNetUpdate && TimeSeconds <= ActorInfo->NextUpdateTime)
				continue;

			auto Actor = ActorInfo->Actor;

			if (!Actor || Actor->RemoteRole == ENetRole::ROLE_None || Actor->bActorIsBeingDestroyed)
				continue;
			if (Actor->NetDormancy == ENetDormancy::DORM_Initial && Actor->bNetStartup)
				continue;
			if (Actor->Name.ComparisonIndex != 0)
			{
				CallPreReplication(Actor, NetDriver);
				OutConsiderList.push_back(ActorInfo);
			}
		}
	}

	static bool IsRelevancyOwnerFor(AActor* InActor, const AActor* ReplicatedActor, const AActor* ActorOwner, AActor* ConnectionActor)
	{
		return (ActorOwner == InActor);
	}

	static UNetConnection* IsActorOwnedByAndRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, bool& bOutHasNullViewTarget)
	{
		const AActor* ActorOwner = Actor->Owner;

		bOutHasNullViewTarget = false;

		for (int i = 0; i < ConnectionViewers.Num(); i++)
		{
			UNetConnection* ViewerConnection = ConnectionViewers[i].Connection;

			if (ViewerConnection->ViewTarget == nullptr)
			{
				bOutHasNullViewTarget = true;
			}

			if (ActorOwner == ViewerConnection->PlayerController ||
				(ViewerConnection->PlayerController && ActorOwner == ViewerConnection->PlayerController->Pawn) ||
				(ViewerConnection->ViewTarget && IsRelevancyOwnerFor(ViewerConnection->ViewTarget, Actor, ActorOwner, ViewerConnection->OwningActor)))
			{
				return ViewerConnection;
			}
		}

		return nullptr;
	}

	bool DistSquared(const FVector& V1, const FVector& V2)
	{
		return Square(V2.X - V1.X) + Square(V2.Y - V1.Y) + Square(V2.Z - V1.Z);
	}

	bool IsWithinNetRelevancyDistance(AActor* Actor, const FVector& SrcLocation)
	{
		return DistSquared(SrcLocation, Actor->K2_GetActorLocation()) < Actor->NetCullDistanceSquared;
	}

	bool IsNetRelevantFor(AActor* Actor, const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation)
	{
		if (Actor->bAlwaysRelevant || Actor->Owner == ViewTarget || Actor->Owner == RealViewer || Actor->Owner == ViewTarget || ViewTarget == Actor->Instigator)
		{
			return true;
		}
		else if (Actor->bNetUseOwnerRelevancy && Actor->Owner)
		{
			return IsNetRelevantFor(Actor->Owner, RealViewer, ViewTarget, SrcLocation);
		}
		else if (Actor->bOnlyRelevantToOwner)
		{
			return false;
		}
		else if (Actor->RootComponent && Actor->RootComponent->GetAttachParent() && Actor->RootComponent->GetAttachParent()->GetOwner() && ((USkeletalMeshComponent*)(Actor->RootComponent->GetAttachParent()) || (Actor->RootComponent->GetAttachParent()->GetOwner() == Actor->Owner)))
		{
			return IsNetRelevantFor(Actor->RootComponent->GetAttachParent()->GetOwner(), RealViewer, ViewTarget, SrcLocation);
		}
		else if (Actor->bHidden && (!Actor->RootComponent || !Actor->RootComponent))
		{
			return false;
		}

		if (!Actor->RootComponent)
		{
			return false;
		}

		return !IsWithinNetRelevancyDistance(Actor, SrcLocation);
	}


	bool IsActorRelevantToConnection(AActor* Actor, const TArray<FNetViewer>& ConnectionViewers)
	{
		for (int32_t viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++)
		{
			if (IsNetRelevantFor(Actor, ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, ConnectionViewers[viewerIdx].ViewLocation))
			{
				return true;
			}
		}

		return false;
	}

	int32_t ServerReplicateActors(float DeltaSeconds)
	{
		if (!Globals::World)
			return 0;

		++*(DWORD*)(BeaconHost->GetNetDriver() + 648);

		int32_t Updated = 0;

		const int32_t NumClientsToTick = ServerReplicateActors_PrepConnections(DeltaSeconds);

		//SULFUR_LOG("ServerReplicateActors: NumClientsToTick: " << NumClientsToTick);

		if (NumClientsToTick == 0)
		{
			return 0;
		}

		AWorldSettings* WorldSettings = Globals::World->PersistentLevel->WorldSettings;

		std::vector<FNetworkObjectInfo*> ConsiderList;

		ServerReplicateActors_BuildConsiderList(BeaconHost->GetNetDriver(), ConsiderList, 0.0f);

		/*if (CVarNetDormancyValidate.GetValueOnAnyThread() == 2)
		{
			for (auto It = Connection->DormantReplicatorMap.CreateIterator(); It; ++It)
			{
				FObjectReplicator& Replicator = It.Value().Get();

				if (Replicator.OwningChannel != nullptr)
				{
					Replicator.ValidateAgainstState(Replicator.OwningChannel->GetActor());
				}
			}
		}*/

		for (int32_t i = 0; i < BeaconHost->GetNetDriver()->ClientConnections.Num(); i++)
		{
			UNetConnection* Connection = BeaconHost->GetNetDriver()->ClientConnections[i];
			if (!Connection) continue;

			if (i >= NumClientsToTick)
			{
				for (int32_t ConsiderIdx = 0; ConsiderIdx < ConsiderList.size(); ConsiderIdx++)
				{
					AActor* Actor = ConsiderList[ConsiderIdx]->Actor;

					if (Actor != NULL && !ConsiderList[ConsiderIdx]->bPendingNetUpdate)
					{
						UActorChannel* Channel = FindChannel(Actor, Connection);

						/*if (Channel != NULL && Channel->LastUpdateTime < ConsiderList[ConsiderIdx]->LastNetUpdateTime)
						{
							ConsiderList[ConsiderIdx]->bPendingNetUpdate = true;
						}*/
					}
				}

				//Connection->TimeSensitive = false;
			}
			else if (Connection->ViewTarget)
			{
				if (Connection->PlayerController)
				{
					//Connection->PlayerController->SendClientAdjustment();
				}

				for (int32_t ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
				{
					if (Connection->Children[ChildIdx]->PlayerController != NULL)
					{
						//Connection->Children[ChildIdx]->PlayerController->SendClientAdjustment();
					}
				}

				for (auto ActorInfo : ConsiderList)
				{
					if (ActorInfo && ActorInfo->Actor)
					{
						if (ActorInfo->Actor->IsA(APlayerController::StaticClass()) && ActorInfo->Actor != Connection->PlayerController)
						{
							continue;
						}

						auto Channel = FindChannel(ActorInfo->Actor, Connection);
						if (!Channel) {
							Channel = (UActorChannel*)CreateChannel(Connection, 2, false, -1);
							SetChannelActor(Channel, ActorInfo->Actor);

							SULFUR_LOG("ReplicatingActor: " << ActorInfo->Actor->GetName());
						}
						ReplicateActorInternal(Channel);
					}
				}
			}
		}

		return Updated;
	}
};