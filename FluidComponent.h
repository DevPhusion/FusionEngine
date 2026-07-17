#pragma once
#include "Component.h"
#include "Object.h"
#include "EngineManager.h"

struct FluidParticle {
    Object* parent;
    glm::vec3 position;
    glm::vec3 predictedPosition;
    glm::vec3 prevPosition;
    glm::vec3 velocity;
    float fluidPressure;
    float invMass;
    float mass;
    float lambda;
    float smoothingRadius;
    float epsilon;
    float poly6Coeff = 0.0f;
    float spikyCoeff = 0.0f;
    glm::vec3 vorticity;
};

class FluidComponent : public ComponentBase<FluidComponent>
{
public:
    FluidComponent(Object* parent);
    FluidComponent() = default;

    std::vector<FluidParticle*> particles;
    glm::vec4 color = glm::vec4(0.2f, 0.5f, 1.0f, 0.8f);
    int desiredParticleCount = 500;
    float particleRadius = 0.05f;

    float particleMass = 1.0f;
    float pressure = 3800.0f;
    float epsilon = 100.0f;
    float smoothingRadius = 0.15f;

    // --- Metaball surface rendering ---
    glm::vec4 outlineColor = glm::vec4(0.05f, 0.2f, 0.45f, 1.0f);
    float metaballThreshold = 0.6f;
    float metaballEdgeSoft = 0.05f;
    float outlineWidthTexels = 2.0f;

    virtual void OnDelete();
    virtual void ProcessInspectorUI();
    virtual void CopyTo(Object* other);
    virtual std::unique_ptr<Component> Clone(Object* parent);
    virtual void Serialize(BinaryWriter& w);
    virtual void Deserialize(BinaryReader& r);
    virtual void SetEnabled(bool enabled);

    void ProcessFluid(float delta);
    void SeedParticles();
    void InitRenderResources();
    void UpdateInstanceBuffer();
    void UpdateParticleTransforms();
    void ResizeInstanceBuffer();
    void Draw();

    // Call this whenever the viewport/window size changes so the density
    // texture stays pixel-matched with the screen.
    void ResizeRenderTargets(int width, int height);

private:
    void RebuildQuadGeometry();

    // Metaball pipeline internals
    void InitDensityFBO(int width, int height);
    void RebuildDensityQuadGeometry();
    void InitFullscreenQuad();
    void DrawDensityPass();
    void DrawComposite();

    int transformCallbackID = -1;
    int setShapeCallbackID = -1;
    std::vector<glm::vec3> localParticlePositions;

    GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
    GLuint instanceVBO = 0;
    Shader particleShader;
    bool renderInitialized = false;

    // Density pass (particles -> soft blob field)
    GLuint densityFBO = 0;
    GLuint densityTex = 0;
    GLuint densityQuadVAO = 0, densityQuadVBO = 0; // bigger quads, shares instanceVBO
    Shader densityShader;
    int densityW = 0, densityH = 0;
    bool densityInitialized = false;

    // Composite pass (density field -> filled shape + outline)
    GLuint fsQuadVAO = 0, fsQuadVBO = 0;
    Shader compositeShader;
};