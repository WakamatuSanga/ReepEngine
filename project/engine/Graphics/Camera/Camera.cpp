#include "Camera.h"
#include "Engine/Core/WinApp.h"
#include <cmath>

using namespace MatrixMath;

Camera::Camera()
	: transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(100.0f)
{
	Update();
}

void Camera::Update() {
	// ワールド行列 (カメラ自体の位置・回転)
	worldMatrix_ = MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

	// ビュー行列 (ワールド行列の逆行列)
	viewMatrix_ = Inverse(worldMatrix_);

	// プロジェクション行列
	projectionMatrix_ = PerspectiveFov(fovY_, aspectRatio_, nearClip_, farClip_);

	// ビュープロジェクション行列 (合成)
	viewProjectionMatrix_ = Multipty(viewMatrix_, projectionMatrix_);
}
