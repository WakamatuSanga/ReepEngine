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

	// 描画専用OffsetはGameplay Cameraの物理座標を変更しない。
	Matrix4x4 viewWorldMatrix = worldMatrix_;
	viewWorldMatrix.m[3][0] += viewTranslationOffset_.x;
	viewWorldMatrix.m[3][1] += viewTranslationOffset_.y;
	viewWorldMatrix.m[3][2] += viewTranslationOffset_.z;
	viewMatrix_ = Inverse(viewWorldMatrix);

	// プロジェクション行列
	projectionMatrix_ = PerspectiveFov(fovY_, aspectRatio_, nearClip_, farClip_);

	// ビュープロジェクション行列 (合成)
	viewProjectionMatrix_ = Multipty(viewMatrix_, projectionMatrix_);
}

void Camera::SetRenderCoordinateFrame(const Camera& source, const Matrix4x4& frameToWorld) {
	*this = source;
	viewMatrix_ = Multipty(frameToWorld, source.GetViewMatrix());
	projectionMatrix_ = source.GetProjectionMatrix();
	viewProjectionMatrix_ = Multipty(viewMatrix_, projectionMatrix_);
	worldMatrix_ = Inverse(viewMatrix_);
	transform_.translate = { worldMatrix_.m[3][0], worldMatrix_.m[3][1], worldMatrix_.m[3][2] };
	viewTranslationOffset_ = {};
}
