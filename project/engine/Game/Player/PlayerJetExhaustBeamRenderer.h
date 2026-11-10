#pragma once
#include "Engine/Graphics/Primitive/BeamBatchRenderer.h"

// Compatibility owner for existing jet/ring users; upload and PSOs are shared.
class PlayerJetExhaustBeamRenderer : public BeamBatchRenderer {};
