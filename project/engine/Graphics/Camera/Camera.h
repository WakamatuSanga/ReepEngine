#pragma once
#include "Matrix4x4.h"

class Camera {
public:
	Camera();

	// 更新
	void Update();
	// Render the same image/depth in another coordinate frame (no source camera mutation).
	void SetRenderCoordinateFrame(const Camera& source, const Matrix4x4& frameToWorld);

	// セッター
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetViewTranslationOffset(const Vector3& offset) {
		viewTranslationOffset_ = offset;
	}
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }

	// ゲッター
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	Vector3 GetViewTranslate() const {
		return {
			transform_.translate.x + viewTranslationOffset_.x,
			transform_.translate.y + viewTranslationOffset_.y,
			transform_.translate.z + viewTranslationOffset_.z };
	}
	const Vector3& GetViewTranslationOffset() const {
		return viewTranslationOffset_;
	}
	float GetFovY() const { return fovY_; }
	float GetAspectRatio() const { return aspectRatio_; }
	float GetNearClip() const { return nearClip_; }
	float GetFarClip() const { return farClip_; }

private:
	Transform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	Vector3 viewTranslationOffset_{};

	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;
};
