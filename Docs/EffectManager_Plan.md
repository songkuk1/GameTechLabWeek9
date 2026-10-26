# 이펙트 매니저 구현 계획

목표 API (게임 로직은 Lua에서 호출)

```
SpawnEffect(이펙트, 위치, 회전, 루프 여부, 지속 시간)
SpawnEffectAttached(이펙트, 붙일 액터, 상대 트랜스폼, 루프 여부, 지속 시간)
SpawnTextEffect(텍스트, 위치, 색, 지속 시간)
```

사용처

| 상황 | 호출 |
|---|---|
| 팝콘을 받음 (반짝임) | `SpawnEffect` |
| 팝콘 아닌 것이 바닥/캐릭터에 부딪힘 (튀김 + "-3") | `SpawnEffect` + `SpawnTextEffect` |
| 먼작귀 발밑 연기 | `SpawnEffectAttached` (루프, 멈출 때 `StopEffect`) |

전체 흐름 한눈에

```
Lua (충돌 이벤트 등)
  └─ SpawnEffect(...)  ──►  FEffectManager (UWorld 멤버)
                               ├─ 풀에서 UParticleSubUVComponent 하나 꺼냄
                               │    └─ ApplyDesc → Activate
                               │         (시뮬레이션은 컴포넌트 자신의 TickComponent가 함)
                               ├─ 매 프레임 Tick: 끝난 이펙트를 풀로 반환
                               └─ 텍스트 이펙트: 매 프레임 화면 좌표로 바꿔 HUD.AddText
```

---

## 1. `UParticleSubUVComponent` 수정

지금 이 컴포넌트는 **끝나지 않는 이미터 한 종류**만 지원한다.

- `TickComponent`에서 `Age >= LifeTime`이면 무조건 `RespawnParticle` → 영원히 방출한다.
- 튜닝 값(`ParticleCount`, `MinLifeTime`, `MinRiseSpeed` …)은 private 하드코딩이고, `ParticleCount`는 생성자에서만 반영된다.
- 속도는 Z(위쪽)로만 나간다. 사방으로 튀는 이펙트를 만들 수 없다.
- `BeginPlay`가 입자마다 `Age`를 랜덤으로 줘서 "이미 흘러가던" 상태로 시작한다.

풀에서 꺼내 쓰려면 **한 번 터지고 끝나는 모드**, **끝났는지 알려주는 함수**, **실행 중에 설정을 바꾸는 함수**가 필요하다.

### 1-1. `FEffectDesc` 구조체

`SpawnEffect`의 "이펙트" 인자가 가리키는 것. 이펙트 한 종류를 만드는 데 필요한 값을 전부 담는다.

```cpp
struct FEffectDesc
{
    FString TexturePath;          // 아틀라스 텍스처
    uint32  Cols = 8, Rows = 8;   // 아틀라스 칸 수
    float   FrameRate = 12.0f;

    int32   ParticleCount = 10;
    float   MinLifeTime = 0.5f, MaxLifeTime = 1.0f;
    FVector MinVelocity, MaxVelocity;  // 축마다 이 범위에서 랜덤
    float   StartScale = 1.0f, EndScale = 1.0f;
    float   FadeStart = 0.8f;          // 수명 비율이 이 값을 넘으면 페이드아웃

    bool    bBurst = false;     // true: Activate 순간 ParticleCount개를 한꺼번에 방출
    bool    bRandomStartFrame;  // 지금처럼 시작 프레임을 랜덤으로 할지 (연기: true, 반짝임: false)
};
```

- 속도를 `float RiseSpeed`에서 `FVector` 범위로 바꾸는 게 핵심이다. 연기는 `Min=(−0.1,−0.1,0.7) Max=(0.1,0.1,0.9)`, 튀김은 `Min=(−2,−2,1) Max=(2,2,3)`처럼 쓴다.
- 튀는 이펙트에 중력이 필요하면 `FVector Acceleration`을 하나 더 두고 Tick에서 `Velocity += Acceleration * dt`를 하면 된다.
- 지금 하드코딩된 값을 기본 Desc로 옮겨두면 기존 `AParticleActor`가 그대로 동작한다.

### 1-2. `ApplyDesc(const FEffectDesc&)`

Desc 값을 컴포넌트에 넣는다.

```cpp
void UParticleSubUVComponent::ApplyDesc(const FEffectDesc& Desc)
{
    SetTexture(Desc.TexturePath);      // UBillboardComponent에서 상속
    SetSubUVSize(Desc.Cols, Desc.Rows);
    SetFrameRate(Desc.FrameRate);
    // 나머지 튜닝 값 복사
}
```

**`SetTexture`가 다른 이펙트에 영향을 주지 않는 이유** ([BillboardComponent.cpp:34](../Source/Runtime/Component/BillboardComponent.cpp))

- 생성자에서는 모든 파티클 컴포넌트가 `"SubUVMaterial"` 하나를 공유한다.
- `SetTexture`는 `Material->bIsInstance`가 false면 `UMaterial::CreateInstance`로 **자기 전용 복사본**을 만든 다음 그 복사본의 `Textures[0]`만 바꾼다.
- 풀의 컴포넌트는 처음 쓸 때 한 번만 복사본이 생기고, 그다음부터는 자기 복사본의 텍스처 포인터만 바뀐다 → 재사용할 때 할당이 없다.
- 반대로 `GetAssetByPath<UMaterial>("SubUVMaterial")->Textures[0]`를 **직접** 고치면 모든 파티클이 같이 바뀐다. 이렇게는 하지 말 것.
- `SetTexture(const FString&)`는 내부에서 `LoadTexture`를 부른다. AssetManager가 캐시하는지 확인하고, 아니라면 Desc를 등록할 때 `UTexture2D*`를 미리 로드해두고 포인터 버전을 쓴다.

### 1-3. `Activate()` / `StopEmitting()` / `IsFinished()` / `bLoop`

```cpp
void Activate();        // 입자 배열 초기화 + 방출 시작
void StopEmitting();    // 더 이상 Respawn하지 않음. 살아 있는 입자는 수명대로 사라짐
void Deactivate();      // 즉시 전부 bAlive=false (풀 반환용)
bool IsFinished() const;// 방출이 멈췄고 살아 있는 입자가 0개
bool bEmitting = false;
```

`Activate`가 할 일

1. `Particles.Reset(); Particles.Reserve(Desc.ParticleCount);`
   TArray는 `Reset`해도 용량을 유지한다. 그래서 몇 번 쓰고 나면 재할당이 일어나지 않는다.
2. 입자를 채운다.
   - `bBurst`: 전부 `RespawnParticle`로 살린다(Age = 0).
   - 연속 방출: `bAlive = false`로 넣어두고, Tick에서 방출 간격마다 하나씩 살린다. 아니면 지금처럼 전부 살리되 Age를 0~수명 사이로 흩는다.
3. `bEmitting = true`, `PrimaryComponentTick.SetTickFunctionEnable(true)`.

**`Activate`에서 입자 상태를 완성해야 하는 이유:** `SpawnEffect`는 보통 `UpdatePendingOverlaps`(충돌 이벤트) 안에서 불린다. 이건 `TickTaskManager`가 컴포넌트 Tick을 다 돌린 **뒤**다 ([World.cpp:117](../Source/Runtime/Engine/World.cpp)). 그래서 새로 켠 이펙트는 이번 프레임에 Tick 없이 바로 렌더된다. 입자 위치와 `bAlive`가 이전 사용 때 값으로 남아 있으면, 한 프레임 동안 엉뚱한 곳에 그려진다.

`TickComponent` 변경점

```cpp
if (Particle.Age >= Particle.LifeTime)
{
    if (bEmitting && bLoop) RespawnParticle(Particle);
    else                    Particle.bAlive = false;
    continue;
}
// bAlive == false인 입자는 갱신도 건너뜀
```

- `bLoop=false`이면 첫 수명이 끝난 입자는 다시 살아나지 않는다 → 결국 전부 죽고 `IsFinished()`가 true가 된다.
- `SceneRenderer`는 이미 `bAlive == false`인 입자를 건너뛴다 ([SceneRenderer.cpp:426](../Source/Runtime/Render/SceneRenderer.cpp)). 렌더 쪽은 고칠 게 없다.
- `IsFinished`는 Tick 안에서 살아 있는 입자 수를 세어두면 O(1)이 된다.

### 1-4. `bPrewarm` 분리

`AddComponent`는 컴포넌트를 만들자마자 `DispatchBeginPlay`를 부른다 ([Actor.cpp](../Source/Runtime/GameFramework/Actor.cpp)의 `AddComponent`). 지금 `BeginPlay`에 있는 랜덤 Age 채우기가 그대로 돌면, 풀에 넣는 순간 입자가 잔뜩 살아 있는 상태가 된다.

- `BeginPlay`의 입자 채우기를 `if (bPrewarm)`으로 감싼다.
- `AParticleActor`가 만든 컴포넌트는 `bPrewarm = true`, `bLoop = true`, `bEmitting = true`로 둔다. 그러면 지금과 똑같이 보인다.
- 풀 컴포넌트는 `bPrewarm = false`라 `BeginPlay`에서 입자를 만들지 않는다.

### 1-5. (선택) 컬링 범위 넓히기

화면에 그릴지는 **입자가 아니라 컴포넌트 단위**로 정한다.

- 컴포넌트의 범위 상자는 `UBillboardComponent::CalcLocalBounds`가 만드는데, 컴포넌트 위치에 사각형 하나 크기다 ([BillboardComponent.cpp:76](../Source/Runtime/Component/BillboardComponent.cpp)).
- 입자는 방출 지점에서 `Velocity * 시간`만큼 월드 공간에서 혼자 움직인다. 범위 상자는 이를 모른다.
- 상자가 화면 밖이면 입자가 화면 안에 있어도 **전부** 안 그려진다.

이번 이펙트들은 퍼지는 범위가 작아서 보통은 문제가 없다. 발밑 연기 꼬리가 화면 가장자리에서 한꺼번에 사라지는 게 거슬리면, `CalcLocalBounds`를 재정의해서 `max(|Velocity|) * MaxLifeTime`만큼 상자를 키운다.

### 1단계 확인

- 기존 ParticleActor 레벨을 열었을 때 예전과 똑같이 보이는지
- 임시 액터에 컴포넌트 하나를 붙이고 `bLoop=false, bBurst=true`로 `Activate` → 한 번 터지고, 다 사라진 뒤 `IsFinished()`가 true가 되는지
- 서로 다른 텍스처를 준 컴포넌트 두 개가 각자 다른 아틀라스로 보이는지

---

## 2. `FEffectManager` 본체

### 2-1. 어디에 둘까: `UWorld` 멤버

`FHUD HUD;`처럼 `FEffectManager EffectManager;`를 `UWorld`에 둔다.

- **싱글톤으로 만들면 안 되는 이유:** 에디터 월드와 PIE 월드가 동시에 존재한다. 싱글톤이면 두 월드가 풀 하나를 같이 쓰게 되고, PIE를 끌 때 PIE 월드의 컴포넌트를 들고 있는 채로 남는다.
- `UObject`일 필요가 없다. 직렬화도 PIE 복제도 필요 없는 실행 중 상태라서 일반 클래스면 충분하다.
- 접근자: `FEffectManager& GetEffectManager() { return EffectManager; }`

### 2-2. 풀 액터와 `Init`

**컴포넌트에 소유 액터가 꼭 필요한 이유**

- `UParticleSubUVComponent::SubmitToRenderQueue`가 `GetOwner()->GetWorld()`를 쓴다.
- Scene 등록(`Scene.AddPrimitive`)과 Tick 등록이 `AActor::AddComponent` 안에서 이루어진다.

그래서 매니저가 **숨은 액터 하나**를 만들고 컴포넌트를 전부 그 액터에 붙여둔다.

```cpp
void FEffectManager::Init(UWorld* InWorld)
{
    World = InWorld;
    PoolOwner = World->SpawnActor<AActor>(FName("EffectPool"));
    for (int32 i = 0; i < InitialPoolSize; ++i)
        AddPooledComponent();      // 아래
}

void FEffectManager::AddPooledComponent()
{
    auto* Comp = Cast<UParticleSubUVComponent>(
        PoolOwner->AddComponent(UParticleSubUVComponent::StaticClass()));
    Comp->Deactivate();            // 입자 전부 bAlive=false
    Comp->PrimaryComponentTick.SetTickFunctionEnable(false);
    // 슬롯 추가, FreeSlots에 넣기
}
```

- **부르는 시점:** `UWorld::BeginPlay`(또는 `NotifyBeginPlay`)에서 `if (IsGameWorld()) EffectManager.Init(this);`. 에디터 월드에서는 만들지 않는다 → 에디터 레벨에 풀 액터가 저장될 일이 없다.
- `AddComponent`는 끝에서 `RegisterAllActorTickFunctions(true)`를 부른다. 그래서 Tick 끄기는 반드시 `AddComponent` **다음에** 한다.
- 풀 액터에 루트 컴포넌트가 없으면 `USceneComponent` 하나를 루트로 만들어둔다. 붙였던 이펙트를 반환할 때 여기로 다시 붙인다.
- `InitialPoolSize`는 동시에 보일 최대 개수 정도로 잡는다. 팝콘 낙하 간격이 0.5–1.2초이고 이펙트 수명이 1초 남짓이면 16개 정도로 충분하다.

### 2-3. 비활성 컴포넌트 처리

풀에서 쉬는 컴포넌트도 Scene에는 primitive로 등록된 채로 있다. 두 가지를 해둔다.

| 처리 | 이유 |
|---|---|
| `PrimaryComponentTick.SetTickFunctionEnable(false)` | 쉬는 동안 입자 루프를 돌리지 않음 |
| 입자 전부 `bAlive=false` (`Deactivate`) | 컬링을 통과해도 `SceneRenderer`가 그릴 입자가 없음 |

`SetHiddenInGame(true)`도 같이 걸어둘 수 있지만, SceneRenderer가 이 플래그를 검사하는지는 확인하지 않았다. `bAlive`는 코드에서 확인한 확실한 경로다.

### 2-4. 이펙트 프리셋 등록

```cpp
TMap<FName, FEffectDesc> Descs;
void RegisterDesc(FName Name, const FEffectDesc& Desc);
```

- `Init`에서 `"PopcornCatch"`, `"HitSplash"`, `"FootSmoke"` 등을 코드로 등록한다. 나중에 필요하면 JSON으로 빼도 된다.
- Lua에서는 이름 문자열로 부른다. 없는 이름이면 로그를 남기고 무효 핸들을 돌려준다.

### 2-5. 슬롯 상태 관리

```cpp
enum class EEffectSlotState : uint8 { Free, Active, Stopping };

struct FEffectSlot
{
    UParticleSubUVComponent* Comp = nullptr;
    EEffectSlotState State = EEffectSlotState::Free;
    uint32 Generation = 0;
    bool   bLoop = false;
    float  Remaining = -1.0f;     // 남은 방출 시간. 음수 = 무한
    AActor* AttachTarget = nullptr;
};
TArray<FEffectSlot> Slots;
TArray<int32> FreeSlots;
```

```
Free ──Spawn──► Active ──(시간 끝 / StopEffect / 한 번만 모드)──► Stopping ──IsFinished──► Free
                  │                                                                 ▲
                  └────────────── StopEffect(bImmediate=true) ──────────────────────┘
```

- **Active:** 방출 중.
- **Stopping:** `StopEmitting()`을 부른 상태. 살아 있는 입자가 수명대로 사라지길 기다린다. 이 단계가 없으면 이펙트가 뚝 끊겨 보인다.
- **Free:** `Deactivate()` + Tick 끄기 + `Generation++` + `FreeSlots`에 반환.

### 2-6. 핸들과 `Generation`

```cpp
struct FEffectHandle { int32 Index = -1; uint32 Generation = 0; bool IsValid() const { return Index >= 0; } };
```

**`Generation`이 막아주는 상황**

1. 발밑 연기를 스폰해서 핸들 `{3, 7}`을 받는다.
2. 무언가 때문에 슬롯 3이 반환되고, 다른 이펙트가 슬롯 3을 다시 쓴다(세대 8).
3. 나중에 Lua가 옛 핸들 `{3, 7}`로 `StopEffect`를 부른다 → 세대가 다르니 무시한다. 남의 이펙트를 끄지 않는다.

`StopEffect`나 `IsEffectActive` 같은 핸들 함수는 모두 `Slots[Index].Generation == Handle.Generation`부터 확인한다.

### 2-7. 풀이 바닥났을 때

하나를 고른다.

| 정책 | 장점 | 단점 |
|---|---|---|
| 최대치까지 늘리기 (`AddPooledComponent`) | 이펙트가 빠지지 않음 | 게임 중에 `NewObject` 발생 |
| 가장 오래된 Active/Stopping 빼앗기 | 메모리 고정 | 오래된 이펙트가 뚝 끊김 |
| 무시하고 무효 핸들 반환 | 가장 단순 | 이펙트가 안 나옴 |

추천: **최대치까지 늘리고, 그 이상이면 무시하면서 로그를 남긴다.** 로그가 찍히면 `InitialPoolSize`를 올린다.

---

## 3. Spawn API

### 3-1. `SpawnEffect`

```cpp
FEffectHandle SpawnEffect(FName Effect, const FVector& Location, const FRotator& Rotation, bool bLoop, float Duration);
```

1. Desc를 찾고 → 빈 슬롯을 하나 꺼낸다.
2. `Comp->SetWorldLocation(...)`와 회전을 설정한다.
3. `Comp->ApplyDesc(Desc)`, `Comp->bLoop = bLoop`, `Comp->Activate()`
4. 슬롯 `State=Active`, `Remaining`을 설정하고 `{Index, Generation}`을 반환한다.

**회전 인자의 의미:** 입자는 항상 카메라를 보는 사각형이다(`View.BuildBillboardMatrix`). 컴포넌트를 돌려도 입자 모양은 돌지 않는다. 회전은 **방출 방향**에만 의미가 있다. `RespawnParticle`에서 랜덤 속도를 컴포넌트 회전으로 돌려서 쓰면 된다. 예: 벽에 맞았을 때 벽 법선 쪽으로 튀게.

**순서 주의:** 위치를 먼저 설정하고 `Activate`를 불러야 한다. `RespawnParticle`이 `GetWorldLocation()`을 그 순간 읽는다.

### 3-2. `SpawnEffectAttached`

```cpp
FEffectHandle SpawnEffectAttached(FName Effect, AActor* Target, const FTransform& Relative, bool bLoop, float Duration);
```

1. 슬롯을 꺼낸다.
2. `Comp->SetupAttachment(Target->GetRootComponent(), EAttachmentRule::KeepRelative)`
3. 상대 위치·회전을 설정한다(발밑이면 `(0, 0, -캐릭터 반높이)` 정도).
4. `ApplyDesc` → `Activate`, `Slot.AttachTarget = Target`

**다른 액터의 컴포넌트에 붙여도 되는 이유 (코드 확인함)**

- 부착 관계는 액터가 아니라 **컴포넌트끼리**의 `AttachParent` / `AttachChildren` 포인터다.
- `USceneComponent::MarkTransformDirty`는 소유 액터와 상관없이 `AttachChildren` 전부에 전파된다.
- `GetWorldMatrix()`는 `AttachParent`를 따라 올라가며 계산한다.
- 그래서 소유는 풀 액터여도 위치는 대상 캐릭터를 따라간다. `GetOwner()->GetWorld()`도 풀 액터의 월드라 문제가 없다.

**입자는 따라오지 않는다:** 컴포넌트(방출 지점)는 따라가지만, 이미 나온 입자는 방출 순간의 월드 좌표에서 혼자 움직인다. 발밑 연기에는 이게 자연스러운 꼬리가 된다.

**반환할 때:** `Comp->SetupAttachment(PoolOwner->GetRootComponent(), ...)`로 다시 붙이고 `AttachTarget = nullptr`. 대상 액터가 파괴될 때의 처리는 5번에서.

### 3-3. `StopEffect`

```cpp
void StopEffect(FEffectHandle Handle, bool bImmediate = false);
```

- 세대가 다르면 무시한다.
- `bImmediate=false`: `Comp->StopEmitting()`, `State=Stopping`. 남은 연기가 자연스럽게 사라진다.
- `bImmediate=true`: 그 자리에서 Free 처리(5번의 `ReleaseSlot`).

### 3-4. `bLoop` / `Duration` 조합

| 인자 | 동작 | 예 |
|---|---|---|
| `bLoop=false` | 한 번 방출. 입자가 다 죽으면(`IsFinished`) 반환. `Duration`은 무시하거나 방출 시간 상한으로만 사용 | 반짝임, 튀김 |
| `bLoop=true, Duration>0` | `Duration`초 동안 방출 → Stopping → 다 죽으면 반환 | 3초간 연기 |
| `bLoop=true, Duration<=0` | `StopEffect`를 부를 때까지 방출 | 걷는 동안 발밑 연기 |

**Duration이 끝났다고 바로 반환하면 안 된다.** 그 순간 살아 있는 입자가 한 프레임 만에 사라진다. Duration은 "방출을 언제 멈추냐"이고, 반환은 "입자가 다 죽었냐"로 판단한다.

---

## 4. `SpawnTextEffect`: HUD로 그리기

### 4-1. HUD를 쓰는 이유

- `UTextRenderComponent`(월드 공간 텍스트)는 **게임에서 그려지지 않는다.** [EditorEngine.cpp:730](../Source/Editor/HitoriEd/EditorEngine.cpp)에서 `TObjectIterator`로 그리는 게 전부이고, 런타임 렌더 파이프라인에는 텍스트 단계가 없다.
- HUD는 게임에서 그려진다(`FSceneRenderPipeline::RenderHUD`). 알파 블렌딩이 켜져 있고 깊이 검사를 안 해서 항상 맨 위에 보인다 ([HUDRenderer.cpp:21](../Source/Runtime/Render/HUDRenderer.cpp), 코드 확인함). `Color.A`로 페이드가 된다.
- **HUD는 매 프레임 비워진다.** `UWorld::Tick` 시작에서 `HUD.BeginFrame()` → `Commands.Reset()`. 그래서 "-3"을 1초 띄우려면 1초 동안 매 프레임 `AddText`를 불러야 한다. 이 상태를 들고 있는 곳이 이펙트 매니저다.

### 4-2. 데이터

```cpp
struct FTextEffect
{
    FString Text;
    FVector WorldLocation;
    FLinearColor Color;
    float Age = 0.0f;
    float Duration = 1.0f;
};
TArray<FTextEffect> TextEffects;
```

`SpawnTextEffect`는 `TextEffects.Add(...)`만 한다. 그리는 일은 `Tick`이 한다.

### 4-3. 매 프레임 갱신 (`Tick` 안에서)

```
for 각 텍스트 이펙트 (뒤에서부터, 지우기 쉽게):
    Age += dt
    Age >= Duration 이면 RemoveAtSwap, continue
    WorldLocation.Z += RiseSpeed * dt                       // 위로 떠오름
    Ratio = Age / Duration
    Alpha = 1 - smoothstep(FadeStart, 1, Ratio)             // 끝부분에서 사라짐
    Screen = WorldToScreen(WorldLocation, ViewProj, VirtualW, VirtualH)
    Screen.X == -FLT_MAX 이면 continue                       // 카메라 뒤
    Screen.X -= 글자수 * FontSize * 0.25                      // 대략 가운데 맞춤
    HUD.AddText(Text, Screen, FontSize, Color with A = Alpha)
```

**좌표 단위 맞추기**

- `WorldToScreen`([Ray.h:43](../Source/Runtime/Collision/Ray.h))은 넘긴 화면 크기 기준 좌표를 돌려준다.
- HUD 명령은 **기준 해상도 단위**(1920×1080 기준)로 받는다. 렌더할 때 `GetScale()`을 곱한다.
- 그래서 화면 크기로 픽셀 크기 대신 `HUD.GetVirtualSize()`를 넘기면 결과가 바로 HUD 단위로 나온다.

**ViewProj 만들기**

- 카메라: `World->GetMainCamera()->GetCameraComponent()`
- 카메라 컴포넌트의 `AspectRatio` 멤버(기본 16:9)가 실제 화면비와 다르면 글자가 옆으로 밀린다.
- `HUD.GetScreenSize()`로 화면비를 구해 `GetProjectionMatrix(화면비)`를 쓴다. 곱하는 순서는 `GetViewProjectionMatrix()` 구현과 똑같이 맞춘다(이 엔진은 `v * ViewProj` 행벡터 방식).

**가운데 맞춤:** HUD에는 정렬 기능이 없다(HUD.cpp에 Todo로 남아 있음). 글자 수는 `FString::size()`가 UTF-8 바이트 수라서, 한글이 섞이면 틀린다. `DecodeUTF8(Text).Num()`([TextRenderer.h](../Source/Runtime/Text/TextRenderer.h))으로 실제 글자 수를 센다. "-3" 같은 숫자만 쓴다면 `size()`로도 된다.

**외곽선/그림자:** HUD 텍스트에는 외곽선이 없다. 검은색으로 (+2, +2) 어긋나게 먼저 한 번 그리고, 원래 색으로 다시 그리면 그림자처럼 보인다.

### 4-4. HUD 방식의 한계 (알고 쓰기)

- 거리에 따라 크기가 변하지 않는다. 필요하면 카메라 거리로 FontSize를 조절한다.
- 벽 뒤에 있어도 보인다(깊이 검사 없음). 데미지 숫자라면 보통 이게 맞다.
- HUD가 월드당 하나라서 화면 하나 기준이다. 화면 분할을 하게 되면 다시 생각해야 한다.

---

## 5. `FEffectManager::Tick`

### 5-1. 부를 위치

`UWorld::Tick`의 게임 월드 블록 안에서, **Lua 코루틴 다음, `FlushPendingKill` 앞**.

```
UWorld::Tick
  Scene.UpdateAllTransforms()
  HUD.BeginFrame()               ← HUD 명령 비움
  BeginPlayList 처리
  TickTaskManager.RunAllTickGroups()   ← 액터·카메라 이동, 파티클 컴포넌트 시뮬레이션
  UpdatePendingOverlaps()        ← 충돌 이벤트 → 여기서 SpawnEffect가 많이 불림
  TickScriptCoroutines()
  EffectManager.Tick(DeltaTime)  ← ★ 여기
  FlushPendingKill()             ← 파괴된 객체 실제 delete
```

이 위치여야 하는 이유

| 이유 | 설명 |
|---|---|
| HUD 명령이 살아남음 | `BeginFrame` 뒤라 이번 프레임에 그려진다 |
| 텍스트가 떨리지 않음 | 카메라와 대상이 이번 프레임 이동을 끝낸 뒤 화면 좌표를 계산한다 |
| 파괴된 대상을 잡을 수 있음 | `DestroyActor`는 즉시 `RF_PendingKill`만 표시하고 실제 delete는 `FlushPendingKill`에서 한다. 그 사이라서 `IsPendingKill()`로 확인할 수 있고 메모리도 아직 유효하다 |

### 5-2. 할 일

```
for 각 슬롯 (Free 제외):
    // (a) 붙은 대상 파괴 처리
    if AttachTarget && AttachTarget->IsPendingKill():
        풀 액터 루트로 다시 붙임 (KeepWorld), AttachTarget = nullptr
        Active였다면 StopEmitting → Stopping

    // (b) 방출 시간
    if Active && Remaining >= 0:
        Remaining -= dt
        if Remaining <= 0: StopEmitting → Stopping
    if Active && !bLoop: Stopping으로 (한 번만 모드는 Activate 직후부터 끝나길 기다리면 됨)

    // (c) 반환
    if Stopping && Comp->IsFinished(): ReleaseSlot(i)

텍스트 이펙트 갱신 (4-3)
```

**(a)가 꼭 필요한 이유 (코드 확인함)**

- `UWorld::DestroyActor`는 대상 액터 **자기 컴포넌트**의 프록시·Tick만 정리한다. 다른 액터 소유인 이펙트 컴포넌트가 붙어 있어도 떼어내지 않는다.
- `FlushPendingKill`이 대상을 delete한 뒤에도 이펙트의 `AttachParent`는 해제된 메모리를 가리킨다.
- 다음 프레임에 `GetWorldLocation()` → `GetWorldMatrix()`가 부모를 따라 올라가다 **크래시**가 난다.
- 떼어낼 때 `KeepWorld`를 쓰면 그 자리에 남아서 남은 연기가 자연스럽게 사라진다.

`ReleaseSlot(i)`

```cpp
Comp->Deactivate();
Comp->PrimaryComponentTick.SetTickFunctionEnable(false);
if (AttachTarget) { 풀 루트로 다시 붙임; AttachTarget = nullptr; }
Slot.State = Free; ++Slot.Generation; FreeSlots.Add(i);
```

---

## 6. 정리 (`Shutdown`)

- **부르는 곳:** `UWorld::EndPlay`, `UWorld::ClearWorld`
- **할 일**
  - `Slots`, `FreeSlots`, `TextEffects`를 비운다.
  - `PoolOwner = nullptr`. 풀 액터 자체는 월드가 정리하면서 지운다. 이미 레벨과 함께 지워질 거라면 `DestroyActor`를 또 부르지 않는다.
  - `World = nullptr`
- `Init`을 두 번 부르는 경우(PIE 재시작 등)를 대비해, `Init` 앞에서 이미 초기화돼 있으면 무시하거나 `Shutdown`부터 부른다.
- 이전 플레이의 핸들은 슬롯이 비워졌으니 `Index` 범위 검사에서 걸러진다. 범위 검사를 빼먹지 말 것.

---

## 7. Lua 바인딩: `LuaBindings_Effect.cpp`

[LuaBindings_HUD.cpp](../Source/Runtime/Script/LuaBindings_HUD.cpp)와 같은 방식으로 만든다.

```cpp
void RegisterLuaEffectAPI(sol::environment& Env, UObject* Owner)
{
    // local h = SpawnEffect("HitSplash", pos [, rot [, loop [, duration]]])
    Env.set_function("SpawnEffect", [Owner](const FString& Name, const FVector& Loc, sol::optional<FRotator> Rot,
                                            sol::optional<bool> bLoop, sol::optional<float> Duration) -> int64 { ... });

    // local h = SpawnEffectAttached("FootSmoke", actor, offset [, loop [, duration]])
    // SpawnTextEffect("-3", pos [, color [, duration]])
    // StopEffect(h [, immediate])
}
```

- **월드 찾기:** `FindHUDForObject`처럼 `Owner` → `AActor` → `GetWorld()`. 게임 월드가 아니면 아무것도 하지 않는다. 공용 헬퍼 `FindEffectManagerForObject`를 만들면 깔끔하다.
- **핸들:** Lua에 구조체를 넘기기보다 `int64 Packed = (int64(Generation) << 32) | Index`로 숫자 하나를 넘긴다. 무효면 `-1`.
- **등록:** 다른 `RegisterLua*API`를 부르는 곳(LuaBindings.cpp 또는 LuaScriptInstance)에 한 줄 추가한다.

### 연결할 곳

| 위치 | 호출 |
|---|---|
| `PopcornSpawner.lua` — 받았을 때 (지금 `print("... got ...")` 하는 곳) | `SpawnEffect("PopcornCatch", 컵 위치)` |
| 팝콘 아닌 것이 바닥/캐릭터에 부딪혔을 때 | `SpawnEffect("HitSplash", 충돌 위치)` + `SpawnTextEffect("-3", 캐릭터 머리 위, 분홍)` |
| `PlayerController.lua` — 이동 시작 | `SmokeHandle = SpawnEffectAttached("FootSmoke", self, 발밑, true)` |
| `PlayerController.lua` — 이동 정지 | `StopEffect(SmokeHandle)` (매 프레임 스폰하지 않도록 시작/정지 순간에만) |

---

## 구현 순서와 단계별 확인

| 순서 | 작업 | 확인 |
|---|---|---|
| 1 | 컴포넌트 수정 (1-1 ~ 1-4) | 기존 ParticleActor 그대로 / 한 번만 모드에서 `IsFinished` |
| 2 | 매니저 뼈대 + 풀 + `SpawnEffect` + Tick 반환 | C++에서 임시로 1초마다 스폰 → 풀 개수가 늘지 않고 재사용되는지 |
| 3 | `SpawnEffectAttached` + 대상 파괴 처리 | 붙인 채 대상 `DestroyActor` → 크래시 없이 연기가 그 자리에서 사라지는지 |
| 4 | `SpawnTextEffect` | 카메라를 돌려도 글자가 대상 위에 붙어 있는지, 카메라 뒤에서 안 보이는지 |
| 5 | Lua 바인딩 + 게임 연결 | 실제 게임에서 세 가지 이펙트 |
| 6 | `Shutdown` | PIE를 여러 번 켰다 꺼도 크래시·누수 없는지 |

## 코드로 확인한 사항

- [x] 다른 액터의 컴포넌트에 붙여도 트랜스폼이 따라간다: `MarkTransformDirty`가 `AttachChildren`에 전파되고, `GetWorldMatrix`가 `AttachParent`를 따라 계산한다.
- [x] `DestroyActor`는 다른 액터 소유의 붙은 컴포넌트를 떼어내지 않는다 → 5-2 (a) 처리가 필요하다.
- [x] `FHUDRenderer`는 알파 블렌딩을 한다 → `Color.A`로 페이드가 된다.
- [x] `SetTexture`는 처음 부를 때 머티리얼 복사본을 만든다 → 이펙트마다 텍스처가 달라도 된다.
- [x] `UAssetManager::LoadTexture`는 같은 경로를 `AssetMap`에 캐시한다 → `SetTexture(경로)`를 매번 불러도 다시 로드하지 않는다.
- [ ] `SceneRenderer`가 `SetHiddenInGame`을 검사하는지 (2-3, `bAlive`로 처리하면 몰라도 됨)
