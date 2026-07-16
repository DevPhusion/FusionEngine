#pragma once
#include "Component.h"
#include "Object.h"
#include "EngineManager.h"

struct FluidParticle {
    glm::vec3 position;
    glm::vec3 predictedPosition; 
    glm::vec3 prevPosition;
    glm::vec3 velocity;
    float invMass;
    float lambda;      
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
private:
    void RebuildQuadGeometry();

    int transformCallbackID = -1;
    int setShapeCallbackID = -1;
    std::vector<glm::vec3> localParticlePositions;

    GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
    GLuint instanceVBO = 0;
    Shader particleShader;
    bool renderInitialized = false;
};

