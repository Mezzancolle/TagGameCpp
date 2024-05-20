// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIBehaviourTreeGameGameMode.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

AEnemyAIController::AEnemyAIController()
{
	BlackboardData = NewObject<UBlackboardData>();
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("Blackboard"));

	BestBallKeyType = NewObject<UBlackboardKeyType_Object>();
	BestBallKeyType->BaseClass = AActor::StaticClass();
	FBlackboardEntry BallEntry;
	BallEntry.EntryName = "BestBall";
	BallEntry.KeyType = BestBallKeyType;

	BlackboardData->Keys.Add(std::move(BallEntry));

	UBlackboardComponent* NewBlackboard;
	UseBlackboard(BlackboardData, NewBlackboard);
	Blackboard = NewBlackboard;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	GoToPlayer = MakeShared<FAivState>(
		[](AAIController* AIController) {
			AIController->MoveToActor(AIController->GetWorld()->GetFirstPlayerController()->GetPawn(),100.0f);
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState>{
			 EPathFollowingStatus::Type State = AIController->GetMoveStatus();

			 if (State == EPathFollowingStatus::Moving)
			 {
				 return nullptr;
			 }

			 AActor* BestBall = Cast<AActor>(Blackboard->GetValueAsObject("BestBall"));

			 if (BestBall)
			 {
				BestBall->AttachToActor(AIController->GetWorld()->GetFirstPlayerController()->GetPawn(),FAttachmentTransformRules::KeepRelativeTransform);
				BestBall->SetActorRelativeLocation(FVector(0, 0, 0));
				BestBall = nullptr;
			 }
				return SearchForBall;
		}
	);

	SearchForBall = MakeShared<FAivState>(
		[this](AAIController* AIController) {
			AGameModeBase* GameMode = AIController->GetWorld()->GetAuthGameMode();
			AAIBehaviourTreeGameGameMode* AIGameMode = Cast<AAIBehaviourTreeGameGameMode>(GameMode);
			const TArray<ABall*>& BallsList = AIGameMode->GetBalls();

			ABall* NearestBall = nullptr;

			for (int32 i = 0; i < BallsList.Num(); i++)
			{
				if (!BallsList[i]->GetAttachParentActor() &&
					( !NearestBall ||
					FVector::Distance(AIController->GetPawn()->GetActorLocation(), BallsList[i]->GetActorLocation()) <
					FVector::Distance(AIController->GetPawn()->GetActorLocation(), NearestBall->GetActorLocation()) ) )
				{
					NearestBall = BallsList[i];
				}
			}

			Blackboard->SetValueAsObject("BestBall", NearestBall);
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {
			if (Blackboard->GetValueAsObject("BestBall"))
			{
				return GoToBall;
			}
			else {
				CurrentState->CallEnter(AIController);
				return SearchForBall;
			}
		}
	);

	GoToBall = MakeShared<FAivState>(
		[this](AAIController* AIController) {
			AActor* BestBall = Cast<AActor>(Blackboard->GetValueAsObject("BestBall"));
			if (BestBall)
			{
				AIController->MoveToActor(BestBall,100.0f);
			}
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {
			EPathFollowingStatus::Type State = AIController->GetMoveStatus();
			AActor* BestBall = Cast<AActor>(Blackboard->GetValueAsObject("BestBall"));
			if (BestBall->GetAttachParentActor())
			{
				return SearchForBall;
			}

			if (State == EPathFollowingStatus::Moving)
			{
				return nullptr;
			}
			return GrabBall;
		}
	);

	GrabBall = MakeShared<FAivState>(
		[this](AAIController* AIController)
		{
			AActor* BestBall = Cast<AActor>(Blackboard->GetValueAsObject("BestBall"));

			if (BestBall->GetAttachParentActor())
			{
				Blackboard->SetValueAsObject("BestBall", nullptr);
			}
		},
		nullptr,
		[this](AAIController* AIController, const float DeltaTime) -> TSharedPtr<FAivState> {

			if (!Blackboard->GetValueAsObject("BestBall"))
			{
				return SearchForBall;
			}

			AActor* BestBall = Cast<AActor>(Blackboard->GetValueAsObject("BestBall"));
			BestBall->AttachToActor(AIController->GetPawn(), FAttachmentTransformRules::KeepRelativeTransform);
			BestBall->SetActorRelativeLocation(FVector(0, 0, 0));

			return GoToPlayer;
		}
	);

	CurrentState = SearchForBall;
	CurrentState->CallEnter(this);
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState = CurrentState->CallTick(this, DeltaTime);
	}
}
