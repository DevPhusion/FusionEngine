#pragma once
#include "Component.h"
#include "Object.h"
#include "EngineManager.h"
#include <unordered_set>

struct FluidParticle {
    Object* parent;
    glm::vec3 position;
    glm::vec3 predictedPosition;
    glm::vec3 velocity;
    float collisionRadius;
    float restDensity;
    float density;
    float viscosity;
    
    float invMass;
    float mass;
    
    float lambda;
    float smoothingRadius;
    
    float epsilon;
    float vorticityEps;

    float bouyancyDensity;
    float bouyancyDamping;
    int bouyancyMinNeighbours;

    float poly6Coeff = 0.0f;
    float spikyCoeff = 0.0f;
};

struct RigidBoundary;
struct SoftBoundary;

class FluidComponent : public ComponentBase<FluidComponent>
{
public:
    FluidComponent(Object* parent);
    FluidComponent() = default;

    std::vector<FluidParticle*> particles;
    glm::vec4 color = glm::vec4(0.2f, 0.5f, 1.0f, 0.8f);
    int desiredParticleCount = 500;
    float particleRadius = 0.5f;
    float collisionRadius = 0.1f;

    float particleMass = 1.0f;
    float restDensity = 400.0f;
    float viscosity = 0.0001f;
    float epsilon = 100.0f;
    float smoothingRadius = 1.0f;
    float vorticityStrength = 0.0f;
    float bouyancyDensity = 10.0f;
    float bouyancyDamping = 5.0f;
    int bouyancyMinNeighbours = 4;
    
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

    void ClearParticles();
    void SeedParticles();
    void InitRenderResources();
    void UpdateInstanceBuffer();
    void UpdateParticleTransforms();
    void ResizeInstanceBuffer();
    void Draw();

    void ResizeRenderTargets(int width, int height);
    std::vector<const RigidBoundary*> GetOverlappingRigidBodies();
    std::vector<const SoftBoundary*> GetOverlappingSoftBodies();

private:
    void RebuildQuadGeometry();

    void InitDensityFBO(int width, int height);
    void RebuildDensityQuadGeometry();
    void InitFullscreenQuad();
    void UpdateHeatBuffer();
    void DrawObjectSilhouette(const RigidBoundary& rb);
    void DrawObjectSilhouette(const SoftBoundary& sb);
    void DrawDensityPass();
    void DrawComposite();
    void DrawParticlesDebug();

    bool IsFullySubmerged(const RigidBoundary& rb);
    bool IsFullySubmerged(const SoftBoundary& sb);

    int transformCallbackID = -1;
    int setShapeCallbackID = -1;
    std::vector<glm::vec3> localParticlePositions;

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