#pragma once

#include "../Renderer/Renderer.h"

class Camera
{
public:
	Camera() = default;
	
	void Initialize(DirectX::XMFLOAT4 &position);
	
	DirectX::XMFLOAT4 * GetPositionPtr();
	DirectX::XMFLOAT4 * GetUpDirectionPtr();
	DirectX::XMFLOAT4 * GetTargetPtr();
	
	void MoveForward();
	void MoveBackward();
	void MoveLeft();
	void MoveRight();
	
	~Camera() = default;

private:

	void UpdateTarget();
	void UpdateRightDirection();

	DirectX::XMFLOAT4 mPosition;
	DirectX::XMFLOAT4 mForwardDirection;
	DirectX::XMFLOAT4 mUpDirection;
	DirectX::XMFLOAT4 mRightDirection;
	DirectX::XMFLOAT4 mTarget;
};