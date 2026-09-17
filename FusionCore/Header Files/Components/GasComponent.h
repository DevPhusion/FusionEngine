#pragma once
#include "Component.h"
#include "../Core/Physics/ContinuumParticle.h"

class GasComponent : public ComponentBase<GasComponent>
{
public:
	GasComponent(Object* parent);
	GasComponent() = default;

    std::vector<GasParticle*> particles;
    glm::vec4 color = glm::vec4(0.2f, 0.5f, 1.0f, 0.8f);
    int desiredParticleCount = 500;
    float particleRadius = 0.5f;
    float collisionRadius = 0.1f;

    float particleMass = 1.0f;
    float restDensity = 1.2f;
    float viscosity = 0.0001f;
    float epsilon = 100.0f;
    float smoothingRadius = 1.0f;
    float vorticityStrength = 0.0f;

    float stiffness = 50.0f;
    float gamma = 1.0f;

	float initialTemperature = 300.0f;
	float ambientTemperature = 300.0f;
	float dissipationRate = 0.15f;

    glm::vec4 outlineColor = glm::vec4(0.05f, 0.2f, 0.45f, 1.0f);
    float metaballThreshold = 0.6f;
    float metaballEdgeSoft = 0.05f;
    float outlineWidthTexels = 2.0f;

    virtual void Activate();
    virtual void Deactivate();
    virtual void OnDelete();
    virtual void ProcessInspectorUI();
    virtual void CopyTo(Object* other);
    virtual void Serialize(BinaryWriter& w);
    virtual void Deserialize(BinaryReader& r);
    virtual void SetEnabled(bool enabled);

    void EnsureGLResources();
    void ClearParticles();
    void SeedParticles();
    void InitRenderResources();
    void UpdateInstanceBuffer();
    void UpdateCollisionLayerMask();
    void UpdateParticleTransforms();
    void ResizeInstanceBuffer();
    void Draw();

    void ResizeRenderTargets(int width, int height);
    void RebuildDensityQuadGeometry();
private:
    void RebuildQuadGeometry();

    void InitDensityFBO(int width, int height);
    void InitFullscreenQuad();
    void InitVectorFieldResources();
    void UpdateHeatBuffer();
    glm::vec4 VelocityHeatmapColor(float t);
    void DrawDensityPass();
    void DrawComposite();
    void DrawParticlesDebug();
    void DrawVelocityField();

    int transformCallbackID = -1;
    int setShapeCallbackID = -1;
    std::vector<glm::vec3> localParticlePositions;

    GLuint vectorFieldVAO = 0, vectorFieldVBO = 0;
    Shader vectorFieldShader;

    unsigned int heatVBO = 0;

    GLuint solidMaskVAO = 0, solidMaskVBO = 0;
    Shader solidMaskShader;

    GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
    GLuint instanceVBO = 0;
    Shader particleShader;
    bool renderInitialized = false;

    GLuint densityFBO = 0;
    GLuint densityTex = 0;
    GLuint densityQuadVAO = 0, densityQuadVBO = 0;
    Shader densityShader;
    int densityW = 0, densityH = 0;
    bool densityInitialized = false;

    GLuint fsQuadVAO = 0, fsQuadVBO = 0;
    Shader compositeShader;
};

