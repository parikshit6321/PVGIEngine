#pragma once

#include "../Renderer/Renderer.h"

class Camera
{
public:
	Camera() = default;
	
	void Initialize(DirectX::XMFLOAT4 &position, float movementSpeed = 10.0f, float rotationSpeed = 10.0f);
	
	DirectX::XMFLOAT4 * GetPositionPtr();
	DirectX::XMFLOAT4 * GetRightDirectionPtr();
	DirectX::XMFLOAT4 * GetUpDirectionPtr();
	DirectX::XMFLOAT4 * GetForwardDirectionPtr();
	DirectX::XMFLOAT4 * GetTargetPtr();
	
	void MoveForward(float deltaTime);
	void MoveBackward(float deltaTime);
	void MoveLeft(float deltaTime);
	void MoveRight(float deltaTime);
	void MoveUp(float deltaTime);
	void MoveDown(float deltaTime);

	void RotateRight(float deltaTime);
	void RotateLeft(float deltaTime);
	void RotateUp(float deltaTime);
	void RotateDown(float deltaTime);
	
	~Camera() = default;

private:

	void UpdateForwardDirection();
	void UpdateRightDirection();
	void UpdateTarget();

	DirectX::XMFLOAT4 mPosition;
	DirectX::XMFLOAT4 mForwardDirection;
	DirectX::XMFLOAT4 mUpDirection;
	DirectX::XMFLOAT4 mRightDirection;
	DirectX::XMFLOAT4 mTarget;

	float mMovementSpeed;
	float mRotationSpeed;
};