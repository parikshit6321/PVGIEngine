#include "Camera.h"

void Camera::Initialize(DirectX::XMFLOAT4 & position)
{
	mPosition = position;
	
	mForwardDirection = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	mUpDirection = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	
	UpdateTarget();
	UpdateRightDirection();
}

DirectX::XMFLOAT4 * Camera::GetPositionPtr()
{
	return &mPosition;
}

DirectX::XMFLOAT4 * Camera::GetUpDirectionPtr()
{
	return &mUpDirection;
}

DirectX::XMFLOAT4 * Camera::GetTargetPtr()
{
	return &mTarget;
}

void Camera::MoveForward()
{
	mPosition.x += mForwardDirection.x * 0.05f;
	mPosition.y += mForwardDirection.y * 0.05f;
	mPosition.z += mForwardDirection.z * 0.05f;

	UpdateTarget();
}

void Camera::MoveBackward()
{
	mPosition.x -= mForwardDirection.x * 0.05f;
	mPosition.y -= mForwardDirection.y * 0.05f;
	mPosition.z -= mForwardDirection.z * 0.05f;

	UpdateTarget();
}

void Camera::MoveLeft()
{
	mPosition.x -= mRightDirection.x * 0.05f;
	mPosition.y -= mRightDirection.y * 0.05f;
	mPosition.z -= mRightDirection.z * 0.05f;

	UpdateTarget();
}

void Camera::MoveRight()
{
	mPosition.x += mRightDirection.x * 0.05f;
	mPosition.y += mRightDirection.y * 0.05f;
	mPosition.z += mRightDirection.z * 0.05f;

	UpdateTarget();
}

void Camera::UpdateTarget()
{
	mTarget.x = mPosition.x + (mForwardDirection.x * 10.0f);
	mTarget.y = mPosition.y + (mForwardDirection.y * 10.0f);
	mTarget.z = mPosition.z + (mForwardDirection.z * 10.0f);
}

void Camera::UpdateRightDirection()
{
	DirectX::XMStoreFloat4(&mRightDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector3Cross(DirectX::XMLoadFloat4(&mUpDirection),
			DirectX::XMLoadFloat4(&mForwardDirection))));
}