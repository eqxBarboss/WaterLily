#include "Engine/Engine.hpp"

#include "Engine/EngineConfig.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/EventSystem.hpp"
#include "Engine/Systems/RenderSystem.hpp"
#include "Engine/Scene/Scene.hpp"

std::unique_ptr<Window> Engine::window;
std::unique_ptr<RenderSystem> Engine::renderSystem;
std::unique_ptr<EventSystem> Engine::eventSystem;

std::unique_ptr<Scene> Engine::scene;

bool Engine::renderingSuspended = false;

void Engine::Create()
{
    eventSystem = std::make_unique<EventSystem>();
    window = std::make_unique<Window>(EngineConfig::windowWidth, EngineConfig::windowHeight, EngineConfig::engineName);

    VulkanContext::Create(*window);

    scene = std::make_unique<Scene>();

    eventSystem->Subscribe<ES::WindowResized>(&Engine::OnResize);
	
    renderSystem = std::make_unique<RenderSystem>(scene.get());
}

void Engine::Run()
{
    while (!window->ShouldClose())
    {
        window->PollEvents();

    	if (!renderingSuspended)
    	{
			renderSystem->Render();
    	}        
    }

    VulkanContext::device->WaitIdle();
}

void Engine::Destroy()
{
    eventSystem->Unsubscribe<ES::WindowResized>(&Engine::OnResize);

    renderSystem.reset();

    scene.reset();

    VulkanContext::Destroy();

    window.reset();
    eventSystem.reset();
}

void Engine::OnResize(const ES::WindowResized& event)
{
    VulkanContext::device->WaitIdle();

    renderingSuspended = event.newWidth == 0 || event.newHeight == 0;

    if (!renderingSuspended)
    {
        VulkanContext::swapchain->Recreate({ event.newWidth, event.newHeight });	
    }
}