#include "Camera.h"

void Camera::Initialize(DirectX::XMFLOAT4 & position, float movementSpeed, float rotationSpeed)
{
	mPosition = position;
	
	mForwardDirection = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	mUpDirection = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	
	mMovementSpeed = movementSpeed;
	mRotationSpeed = rotationSpeed;

	UpdateTarget();
	UpdateRightDirection();
}

DirectX::XMFLOAT4 * Camera::GetPositionPtr()
{
	return &mPosition;
}

DirectX::XMFLOAT4 * Camera::GetRightDirectionPtr()
{
	return &mRightDirection;
}

DirectX::XMFLOAT4 * Camera::GetUpDirectionPtr()
{
	return &mUpDirection;
}

DirectX::XMFLOAT4 * Camera::GetForwardDirectionPtr()
{
	return &mForwardDirection;
}

DirectX::XMFLOAT4 * Camera::GetTargetPtr()
{
	return &mTarget;
}

void Camera::MoveForward(float deltaTime)
{
	mPosition.x += mForwardDirection.x * mMovementSpeed * deltaTime;
	mPosition.y += mForwardDirection.y * mMovementSpeed * deltaTime;
	mPosition.z += mForwardDirection.z * mMovementSpeed * deltaTime;

	UpdateTarget();
}

void Camera::MoveBackward(float deltaTime)
{
	mPosition.x -= mForwardDirection.x * mMovementSpeed * deltaTime;
	mPosition.y -= mForwardDirection.y * mMovementSpeed * deltaTime;
	mPosition.z -= mForwardDirection.z * mMovementSpeed * deltaTime;

	UpdateTarget();
}

void Camera::MoveLeft(float deltaTime)
{
	mPosition.x -= mRightDirection.x * mMovementSpeed * deltaTime;
	mPosition.y -= mRightDirection.y * mMovementSpeed * deltaTime;
	mPosition.z -= mRightDirection.z * mMovementSpeed * deltaTime;

	UpdateTarget();
}

void Camera::MoveRight(float deltaTime)
{
	mPosition.x += mRightDirection.x * mMovementSpeed * deltaTime;
	mPosition.y += mRightDirection.y * mMovementSpeed * deltaTime;
	mPosition.z += mRightDirection.z * mMovementSpeed * deltaTime;

	UpdateTarget();
}

void Camera::MoveUp(float deltaTime)
{
	mPosition.x += mUpDirection.x * mMovementSpeed * deltaTime;
	mPosition.y += mUpDirection.y * mMovementSpeed * deltaTime;
	mPosition.z += mUpDirection.z * mMovementSpeed * deltaTime;

	UpdateTarget();
}

void Camera::MoveDown(float deltaTime)
{
	mPosition.x -= mUpDirection.x * mMovementSpeed * deltaTime;
	mPosition.y -= mUpDirection.y * mMovementSpeed * deltaTime;
	mPosition.z -= mUpDirection.z * mMovementSpeed * deltaTime;

	UpdateTarget();
}

void Camera::RotateRight(float deltaTime)
{
	DirectX::XMStoreFloat4(&mForwardDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector4Transform(XMLoadFloat4(&mForwardDirection),
			DirectX::XMMatrixRotationAxis(XMLoadFloat4(&mUpDirection), mRotationSpeed * deltaTime))));

	UpdateRightDirection();
	UpdateTarget();
}

void Camera::RotateLeft(float deltaTime)
{
	DirectX::XMStoreFloat4(&mForwardDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector4Transform(XMLoadFloat4(&mForwardDirection),
		DirectX::XMMatrixRotationAxis(XMLoadFloat4(&mUpDirection), -1.0f * mRotationSpeed * deltaTime))));
	
	UpdateRightDirection();
	UpdateTarget();
}

void Camera::RotateUp(float deltaTime)
{
	DirectX::XMStoreFloat4(&mUpDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector4Transform(XMLoadFloat4(&mUpDirection),
			DirectX::XMMatrixRotationAxis(XMLoadFloat4(&mRightDirection), -1.0f * mRotationSpeed * deltaTime))));

	UpdateForwardDirection();
	UpdateTarget();
}

void Camera::RotateDown(float deltaTime)
{
	DirectX::XMStoreFloat4(&mUpDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector4Transform(XMLoadFloat4(&mUpDirection),
			DirectX::XMMatrixRotationAxis(XMLoadFloat4(&mRightDirection), mRotationSpeed * deltaTime))));

	UpdateForwardDirection();
	UpdateTarget();
}

void Camera::UpdateForwardDirection()
{
	DirectX::XMStoreFloat4(&mForwardDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector3Cross(DirectX::XMLoadFloat4(&mRightDirection),
			DirectX::XMLoadFloat4(&mUpDirection))));
}

void Camera::UpdateRightDirection()
{
	DirectX::XMStoreFloat4(&mRightDirection, DirectX::XMVector4Normalize(
		DirectX::XMVector3Cross(DirectX::XMLoadFloat4(&mUpDirection),
			DirectX::XMLoadFloat4(&mForwardDirection))));
}

void Camera::UpdateTarget()
{
	mTarget.x = mPosition.x + (mForwardDirection.x * 10.0f);
	mTarget.y = mPosition.y + (mForwardDirection.y * 10.0f);
	mTarget.z = mPosition.z + (mForwardDirection.z * 10.0f);
}