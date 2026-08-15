#include "../../../Header Files/Core/Rendering/Renderer.h"

void Renderer::Setup(std::vector<std::unique_ptr<Object>>* objects) {
    this->allObjects = objects;
    gizmos = new Gizmos();
    gizmos->Initialize();
	polygonEditGizmos = new PolygonEditGizmos();
    constraintEditGizmos = new ConstraintEditGizmos();
}

void Renderer::Draw() {
    TIME_BLOCK("Rendering");

    glm::vec2 camPos = glm::vec2(Camera::getInstance().cameraPos.x, Camera::getInstance().cameraPos.y);

    glm::vec2 screenSize = glm::vec2(EngineManager::getInstance().resolutionWidth, EngineManager::getInstance().resolutionHeight);
    float zoom = Camera::getInstance().cameraZoom;

    auto& debug = EngineManager::getInstance().EngineSettings;

    if (debug.drawBackgroundGrid && EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate) {
        {
            TIME_BLOCK("Draw background grid");
            backgroundGrid.Draw(camPos, screenSize, zoom);
        }
    }

    if (debug.drawObjectWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    {
        TIME_BLOCK("Draw constraint display");
        constraintEditGizmos->DrawConstraintDisplays();
    }

    std::vector<Object*> renderQueue;

    {
        TIME_BLOCK("Construct draw queue");
        for (size_t i = 0; i < this->allObjects->size(); i++) {
            if ((*allObjects)[i]->hidden) continue;

            if ((*allObjects)[i]->HasComponent<RenderComponent>()) {
                renderQueue.push_back((*allObjects)[i].get());
            }

            if ((*allObjects)[i]->HasComponent<EditorRenderComponent>() &&
                EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate) {
                renderQueue.push_back((*allObjects)[i].get());
            }
        }
        std::sort(renderQueue.begin(), renderQueue.end(), [](Object* a, Object* b) {
            float zA = 0.0f;
            float zB = 0.0f;

            if (a->HasComponent<RenderComponent>()) {
                zA = a->GetComponent<RenderComponent>()->z_index;
            }
            else {
                zA = a->GetComponent<EditorRenderComponent>()->z_index;
            }
            if (b->HasComponent<RenderComponent>()) {
                zB = b->GetComponent<RenderComponent>()->z_index;
            }
            else {
                zB = b->GetComponent<EditorRenderComponent>()->z_index;
            }

            return zA < zB;
            });
    }  

    for (Object* obj : renderQueue) {
        if (obj->HasComponent<FluidComponent>()) {
            {
                TIME_BLOCK("Draw fluids");
                obj->GetComponent<FluidComponent>()->Draw();
            }
        }
        else {
            {
                TIME_BLOCK("Draw objects");
                if (obj->HasComponent<RenderComponent>()) {
                    obj->GetComponent<RenderComponent>()->Draw();
                }
                else {
                    obj->GetComponent<EditorRenderComponent>()->Draw();
                }
            }
        }
        
        if (obj->HasComponent<TransformComponent>()) {
            obj->GetComponent<TransformComponent>()->ProcessTransform();
        }
        SoftBodyComponent* sb = obj->GetComponent<SoftBodyComponent>();
        if (sb) {
            for (int i = 0; i < sb->MassAggregate.size(); i++)
            {
                sb->MassAggregate[i]->ProcessTransform();
            }
        }
    }

    if (EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate) {
        {
            TIME_BLOCK("Draw camera bounds");
            for (size_t i = 0; i < (*allObjects).size(); i++) {
                CameraComponent* camComp = (*allObjects)[i]->GetComponent<CameraComponent>();
                if (camComp) {
                    camComp->DrawDebug();
                }
            }
        }
    }

    if (EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate ||
        debug.drawCollisionShapes) {
        {
            TIME_BLOCK("Draw collision shapes");
            for (size_t i = 0; i < (*allObjects).size(); i++) {
                CollisionComponent* cc = (*allObjects)[i]->GetComponent<CollisionComponent>();
                if (cc) {
                    cc->Draw();
                }
            }
        }
    }

    if (debug.AnyDebugGizmoEnabled()) {
        {
            TIME_BLOCK("Draw debug");
            glLineWidth(2.0f);

            if (debug.drawBroadPhaseBounds) {
                {
                    TIME_BLOCK("Draw bounding area");
                    BAHNode<BoundingCircle>* bvhRoot = &PhysicsEngine::getInstance().root;
                    bvhRoot->DrawBoundingArea();
                }
            }

            if (debug.drawContactPoints || debug.drawCollisionNormals) {
                for (int i = 0; i < PhysicsEngine::getInstance().allContactPoints.size(); i++)
                {
                    ContactPoint& cp = PhysicsEngine::getInstance().allContactPoints[i];

                    if (debug.drawContactPoints) {
                        {
                            TIME_BLOCK("Draw contact points");
                            DebugPoint point = DebugPoint();
                            point.DrawPoint(cp.point, 15, Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));
                        }
                    }

                    if (debug.drawCollisionNormals) {
                        {
                            TIME_BLOCK("Draw collision normal");
                            glm::vec4 normalColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                            float arrowLength = 0.5f;
                            DrawArrow(cp.point, cp.normal, arrowLength, normalColor);
                        }         
                    }
                }
            }

            if (debug.drawSoftBodyPointMasses || debug.drawSoftBodySprings || debug.drawVirtualSoftBodyProxies) {
                for (int i = 0; i < (*allObjects).size(); i++)
                {
                    SoftBodyComponent* sb = (*allObjects)[i]->GetComponent<SoftBodyComponent>();
                    if (sb) {                        
                        if (debug.drawSoftBodySprings) {
                            {
                                TIME_BLOCK("Draw soft body springs");
                                sb->DrawSprings();
                            }
                        }
                        if (debug.drawSoftBodyPointMasses) {
                            {
                                TIME_BLOCK("Draw soft body point masses");
                                for (int j = 0; j < sb->MassAggregate.size(); j++)
                                {
                                    sb->MassAggregate[j]->DrawDebug();
                                }
                            }
                        }
                        if (debug.drawVirtualSoftBodyProxies) {
                            {
                                TIME_BLOCK("Draw soft body proxies");
                                for (int j = 0; j < sb->VirtualProxies.size(); j++)
                                {
                                    sb->VirtualProxies[j]->DrawDebug();
                                }
                            }
                        }
                    }
                }
            }
            glLineWidth(1.0f);
        }
    }

    {
        TIME_BLOCK("Draw gizmos");
        gizmos->UpdateGizmos();
        polygonEditGizmos->UpdateGizmos();
        constraintEditGizmos->DrawPivotHandles();
    }
}

void Renderer::DrawLine(glm::vec3 p1, glm::vec3 p2, glm::vec4 color, float thickness, bool screenSpace) {
    glLineWidth(thickness);
    static unsigned int lineVAO = 0;
    static unsigned int lineVBO = 0;
    static unsigned int whiteTex = 0;
    static Shader lineShader = Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt");

    if (lineVAO == 0) {
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);

        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, 2 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        glGenTextures(1, &whiteTex);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glm::vec3 renderP2 = p2;
    if (screenSpace) {
        float zoom = Camera::getInstance().cameraZoom;
        if (zoom > 1e-6f) {
            glm::vec3 delta = (p2 - p1) * zoom;
            renderP2 = p1 + delta;
        }
    }

    float vertices[6] = { p1.x, p1.y, p1.z, renderP2.x, renderP2.y, renderP2.z };
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    lineShader.use();
    lineShader.setVec4D("aColor", color);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTex);

    glm::mat4 identity(1.0f);
    glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio,
        EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);

    lineShader.setMat4D("projection", projection);
    lineShader.setMat4D("transform", identity);
    lineShader.setMat4D("view", Camera::getInstance().viewMatrix);

    glBindVertexArray(lineVAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void Renderer::DrawArrow(glm::vec3 origin, glm::vec3 direction, float length, glm::vec4 color, float thickness,
    float headLength, float headAngleDeg, bool screenSpace) {
    if (glm::length(direction) < 1e-8f) return;
    glm::vec3 dir = glm::normalize(direction);

    float zoom = 1.0f;
    if (screenSpace) {
        zoom = Camera::getInstance().cameraZoom;
        if (zoom < 1e-6f) zoom = 1.0f;
    }

    glm::vec3 tip = origin + dir * (length * zoom);

    DrawLine(origin, tip, color, thickness);

    glm::vec3 back = -dir;
    float rad = glm::radians(headAngleDeg);

    auto rotateZ = [](glm::vec3 v, float angle) {
        float c = cos(angle);
        float s = sin(angle);
        return glm::vec3(v.x * c - v.y * s, v.x * s + v.y * c, v.z);
        };

    glm::vec3 headDirA = rotateZ(back, rad);
    glm::vec3 headDirB = rotateZ(back, -rad);

    float effHeadLength = headLength * zoom;

    glm::vec3 headA = tip + headDirA * effHeadLength;
    glm::vec3 headB = tip + headDirB * effHeadLength;

    DrawLine(tip, headA, color, thickness);
    DrawLine(tip, headB, color, thickness);
}

void Renderer::DrawCircle(glm::vec3 center, float radius, glm::vec4 color, int segments, float thickness, bool screenSpace) {
    float zoom = 1.0f;
    if (screenSpace) {
        zoom = Camera::getInstance().cameraZoom;
        if (zoom < 1e-6f) zoom = 1.0f;
    }
    float effRadius = radius * zoom;

    float angleStep = 2.0f * glm::pi<float>() / (float)segments;

    for (int i = 0; i < segments; ++i) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        glm::vec3 p1 = center + glm::vec3(cos(angle1) * effRadius, sin(angle1) * effRadius, 0.0f);
        glm::vec3 p2 = center + glm::vec3(cos(angle2) * effRadius, sin(angle2) * effRadius, 0.0f);

        DrawLine(p1, p2, color, thickness);
    }
}

void Renderer::DrawFilledPolygon(const std::vector<glm::vec3>& worldPoints, glm::vec4 fillColor, glm::vec4 outlineColor, float outlineThickness) {
    if (worldPoints.size() < 3) return;

    static unsigned int polyVAO = 0;
    static unsigned int polyVBO = 0;
    static unsigned int whiteTex = 0;
    static Shader polyShader = Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt");

    if (polyVAO == 0) {
        glGenVertexArrays(1, &polyVAO);
        glGenBuffers(1, &polyVBO);

        glBindVertexArray(polyVAO);
        glBindBuffer(GL_ARRAY_BUFFER, polyVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        glGenTextures(1, &whiteTex);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glm::vec3 centroid(0.0f);
    for (auto& p : worldPoints) centroid += p;
    centroid /= (float)worldPoints.size();

    std::vector<float> verts;
    verts.reserve((worldPoints.size() + 2) * 3);
    verts.insert(verts.end(), { centroid.x, centroid.y, centroid.z });
    for (auto& p : worldPoints) verts.insert(verts.end(), { p.x, p.y, p.z });
    verts.insert(verts.end(), { worldPoints[0].x, worldPoints[0].y, worldPoints[0].z });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    polyShader.use();
    polyShader.setVec4D("aColor", fillColor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTex);

    glm::mat4 identity(1.0f);
    glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio,
        EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
    polyShader.setMat4D("projection", projection);
    polyShader.setMat4D("transform", identity);
    polyShader.setMat4D("view", Camera::getInstance().viewMatrix);

    glBindVertexArray(polyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, polyVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)(verts.size() / 3));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    for (size_t i = 0; i < worldPoints.size(); i++) {
        DrawLine(worldPoints[i], worldPoints[(i + 1) % worldPoints.size()], outlineColor, outlineThickness);
    }
}