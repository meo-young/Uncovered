#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Interact/UCInteractableObject.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Misc/ScopeExit.h"
#include "UI/Widget/Crosshair/UCCrosshairWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCInteractionCollisionTest, "Uncovered.Interaction.Collision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCInteractionCollisionTest::RunTest(const FString& Parameters)
{
	// 프로젝트 설정과 공통 액터를 함께 로드하여 상호작용 채널의 실제 충돌 동작을 검사합니다.
	TestEqual(TEXT("Interaction channel name"), UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel1), FName(TEXT("Interactable")));
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// 인터페이스 이벤트가 실제 플레이와 동일하게 실행되도록 월드의 액터 초기화를 완료합니다.
	World->InitializeActorsForPlay(FURL());
	AUCInteractableObject* Target = World->SpawnActor<AUCInteractableObject>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	FHitResult Hit;
	TestTrue(TEXT("Interaction box is detected"), World->LineTraceSingleByChannel(Hit, FVector::ZeroVector, FVector(300.0f, 0.0f, 0.0f), ECC_GameTraceChannel1));
	TestEqual(TEXT("Detected actor implements interface"), Hit.GetActor(), static_cast<AActor*>(Target));
	TestTrue(TEXT("Blueprint interaction interface is inherited"), Target->Implements<UInteractable>());
	TestTrue(TEXT("Base interactable is available by default"), IInteractable::Execute_CanInteract(Target));
	TestFalse(TEXT("Out of range target is excluded"), World->LineTraceSingleByChannel(Hit, FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), ECC_GameTraceChannel1));
	TestFalse(TEXT("Interaction box does not block visibility"), World->LineTraceSingleByChannel(Hit, FVector::ZeroVector, FVector(300.0f, 0.0f, 0.0f), ECC_Visibility));

	// 일반 벽의 BlockAll 프로필이 상호작용 대상을 가리고 첫 충돌 대상으로 반환되는지 검사합니다.
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* WallCollision = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(WallCollision);
	WallCollision->InitBoxExtent(FVector(10.0f, 100.0f, 100.0f));
	WallCollision->SetCollisionProfileName(TEXT("BlockAll"));
	WallCollision->RegisterComponent();
	Wall->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	TestTrue(TEXT("Wall blocks interaction trace"), World->LineTraceSingleByChannel(Hit, FVector::ZeroVector, FVector(300.0f, 0.0f, 0.0f), ECC_GameTraceChannel1));
	TestEqual(TEXT("Wall is hit before target"), Hit.GetActor(), Wall);

	// 대상이 제거된 이후에는 이전 충돌 대상이 다시 검출되지 않는지 검사합니다.
	Wall->Destroy();
	Target->Destroy();
	TestFalse(TEXT("Destroyed target is excluded"), World->LineTraceSingleByChannel(Hit, FVector::ZeroVector, FVector(300.0f, 0.0f, 0.0f), ECC_GameTraceChannel1));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCInteractionInputTest, "Uncovered.Interaction.InputMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCInteractionInputTest::RunTest(const FString& Parameters)
{
	// 저장된 입력 에셋을 새로 로드하여 상호작용 액션과 E키 매핑이 유지되는지 검사합니다.
	const UInputAction* Action = LoadObject<UInputAction>(nullptr, TEXT("/Game/_Uncovered/Input/IA_Interact.IA_Interact"));
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/_Uncovered/Input/IMC_Default.IMC_Default"));
	if (!TestNotNull(TEXT("Interaction action asset"), Action) || !TestNotNull(TEXT("Default mapping context asset"), Context))
	{
		return false;
	}
	TestEqual(TEXT("Interaction action uses a digital value"), Action->ValueType, EInputActionValueType::Boolean);
	TestTrue(TEXT("E key maps to interaction action"), Context->GetMappings().ContainsByPredicate([Action](const FEnhancedActionKeyMapping& Mapping)
	{
		return Mapping.Action == Action && Mapping.Key == EKeys::E;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCCrosshairStateTest, "Uncovered.Interaction.CrosshairStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCCrosshairStateTest::RunTest(const FString& Parameters)
{
	// 실제 위젯과 Slate 구성을 생성하여 최초 표시와 상태 전환을 함께 검사합니다.
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		// 검사 실패 여부와 무관하게 임시 월드와 컨텍스트를 정리합니다.
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	UUCCrosshairWidget* Widget = CreateWidget<UUCCrosshairWidget>(World, UUCCrosshairWidget::StaticClass());
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	UImage* Image = Cast<UImage>(Widget->GetWidgetFromName(TEXT("CrosshairImage")));
	if (!TestNotNull(TEXT("Crosshair image is constructed"), Image))
	{
		return false;
	}
	UTexture2D* Normal = LoadObject<UTexture2D>(nullptr, TEXT("/Game/_Uncovered/Texture/Crosshair/T_Crosshair_Normal.T_Crosshair_Normal"));
	UTexture2D* Available = LoadObject<UTexture2D>(nullptr, TEXT("/Game/_Uncovered/Texture/Crosshair/T_Crosshair_Available.T_Crosshair_Available"));
	UTexture2D* Unavailable = LoadObject<UTexture2D>(nullptr, TEXT("/Game/_Uncovered/Texture/Crosshair/T_Crosshair_Unavailable.T_Crosshair_Unavailable"));
	TestEqual(TEXT("Initial state displays normal texture"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Normal));

	// 가능한 상태와 불가능한 상태를 오가며 각 텍스처가 적용되는지 검사합니다.
	Widget->SetInteractionState(EUCInteractionState::Available);
	TestEqual(TEXT("Available texture is displayed"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Available));
	Widget->SetInteractionState(EUCInteractionState::Unavailable);
	TestEqual(TEXT("Unavailable texture is displayed"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Unavailable));
	Widget->SetInteractionState(EUCInteractionState::Available);
	TestEqual(TEXT("Target becoming available refreshes texture"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Available));

	// 브러시를 임시로 변경한 뒤 같은 상태를 전달하여 불필요한 재설정이 발생하지 않는지 검사합니다.
	Image->SetBrushFromTexture(Normal);
	Widget->SetInteractionState(EUCInteractionState::Available);
	TestEqual(TEXT("Unchanged state does not reset brush"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Normal));

	// 위젯을 재구성하면 캐시된 현재 상태를 다시 적용하고 대상 해제 시 기본 표시로 돌아가는지 검사합니다.
	SlateWidget.Reset();
	Widget->ReleaseSlateResources(true);
	SlateWidget = Widget->TakeWidget();
	TestEqual(TEXT("Rebuild reapplies current state"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Available));
	Widget->SetInteractionState(EUCInteractionState::None);
	TestEqual(TEXT("No target restores normal texture"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Normal));
	return true;
}

#endif
