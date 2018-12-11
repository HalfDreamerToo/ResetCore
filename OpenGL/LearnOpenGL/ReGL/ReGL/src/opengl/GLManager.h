﻿/**
 * OpenGL渲染相关函数
 */

#ifndef __GLMANAGER_H__
#define __GLMANAGER_H__
#include "Camera.h"
#include "looper/Scene.h"
#include "looper/IRenderManager.h"

namespace ReGL
{
    class GLManager : public IRenderManager
    {
    public:
        bool Init() override;
        bool WillRender() override;
        bool Update() override;
        bool LateUpdate() override;
        bool Uninit() override;

        const Camera& CurrentCamera() override;
        const Scene& CurrentScene() override;
    private:
        Camera* camera_{ nullptr };
        Scene* scene_{ nullptr };
    };

}

#endif

