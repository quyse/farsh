#include "Character.hpp"
#include "BoneAnimation.hpp"

Character::Character(ptr<CharacterInfo> info, ptr<Physics::World> physicsWorld, const float3& position)
: info(info), referencePosition(position), alpha(0)
{
	animationFrame = NEW(BoneAnimationFrame(info->animation));

	physicsCharacter = physicsWorld->CreateCharacter(info->physicsShape, CreateTranslationMatrix(position));
}

float3 Character::GetPhysicalPosition() const
{
	float4x4 transform = physicsCharacter->GetTransform();
	return float3(transform.t[3][0], transform.t[3][1], transform.t[3][2]);
}

void Character::Tick(float time)
{
	// вычислить текущую ориентацию в виде кватерниона
	quaternion orientation(float3(0, 0, 1), alpha);

	// выяснить, не завершаем ли мы анимацию
	while(animationTime >= info->animationIntervals[aiNumber].end)
	{
		// вычислить кадр анимации на конце интервала
		animationFrame->Setup(referencePosition, orientation, animationTime);
		// получить позицию в этом конце
		float3 endPosition = animationFrame->animationWorldPositions[0];
		// теперь там будет референсная позиция
		referencePosition = endPosition;

		float moreTime = animationTime - info->animationIntervals[aiNumber].end;
		// выбрать новую анимацию
		SelectAnimationInterval();
		// установить время анимации
		animationTime = info->animationIntervals[aiNumber].begin + moreTime;
	}

	// обработать кадр
	Process();

	// вычислить кадр анимации
	animationFrame->Setup(referencePosition, orientation, animationTime);
	// задать перемещение персонажу
	float3 move = animationFrame->animationWorldPositions[0] - GetPhysicalPosition();
	move.z = 0; // перемещение по высоте игнорируется
	physicsCharacter->Walk(move);
}

void Character::Draw(Painter* painter)
{
	painter->AddSkinnedModel(info->material, info->geometry, info->shadowGeometry, animationFrame);
}

//******* Zombie

const float Zombie::attackLength = 2.0f;

Zombie::Zombie(ptr<CharacterInfo> info, ptr<Physics::World> physicsWorld, const float3& position, ptr<Character> target)
: Character(info, physicsWorld, position), target(target), aiNumber(aiRun) {}

bool Zombie::CanAttack() const
{
	return length(GetPhysicalPosition() - target->GetPhysicalPosition()) < attackLength;
}

void Zombie::SelectAnimationInterval()
{
	// если мы можем атаковать
	if(CanAttack())
		// атакуем
		aiNumber = aiAttack;
	else
		// иначе идём
		aiNumber = aiRun;
}

void Zombie::Process()
{
	// если мы уже не атакуем, но можем атаковать
	if(aiNumber != aiAttack && CanAttack())
	{
		// начать атаку
		aiNumber = aiAttack;
		animationTime = info->animationIntervals[aiNumber].begin;
	}
}

//******* Hero

Hero::Hero(ptr<CharacterInfo> info, ptr<Physics::World> physicsWorld, const float3& position)
: Character(info, physicsWorld, position), aiNumber(aiRun) {}

void Hero::SelectAnimationInterval()
{
}

void Hero::Process()
{
	alpha += controlAlpha;
	beta += controlBeta;

	float3 position = GetPhysicalPosition();
	cameraTransform = CreateLookAtMatrix(position, position + 
}

void Hero::Control(float controlAlpha, float controlBeta, bool controlMoveLeft, bool controlMoveRight)
{
	alpha += controlAlpha;
	beta += controlBeta;

	float3 cameraMove(0, 0, 0);
	float3 moveDirectionFront(-sin(cameraAlpha), -cos(cameraAlpha), 0);
	float3 moveDirectionUp(0, 0, 1);
	float3 moveDirectionRight = cross(moveDirectionFront, moveDirectionUp);
	referencePosition += moveDirectionFront
}

const float4x4& Hero::GetCameraViewTransform() const
{
	return cameraViewTransform;
}
