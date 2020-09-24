#include "VEditor.h"
#include <Engine.h>
#include <Components/TransformationComponent.h>
#include <Components/CameraComponent.h>
#include <Graphics/RenderSystem.h>
#include <Graphics/imgui/imgui.h>
//#include <Graphics/imgui/imgui_impl_glfw.h>
#include <Graphics/imgui/ImGuizmo.h>
#include <Window/Window.h>
#include "EntityDetailWindow.h"
#include "EntityWindow.h"
#include "AssetBrowserWindow.h"
#include <Input/ImGuiInputAdapter.h>
#include <Input/UserInput.h>

using namespace VEngine;

VEditor::VEditor::VEditor(VEngine::IGameLogic &gameLogic)
	:m_gameLogic(gameLogic),
	m_engine(),
	m_editorCameraEntity(),
	m_editorImGuiContext(),
	m_imguiInputAdapter(),
	m_userInput(),
	m_entityDetailWindow(),
	m_entityWindow(),
	m_assetBrowserWindow(),
	m_editorMode(true)
{
}

void VEditor::VEditor::initialize(VEngine::Engine *engine)
{
	m_engine = engine;

	// create editor imgui context
	{
		auto *prevImGuiContext = ImGui::GetCurrentContext();

		IMGUI_CHECKVERSION();
		m_editorImGuiContext = ImGui::CreateContext();

		ImGui::SetCurrentContext(m_editorImGuiContext);

		ImGuiIO &io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		io.ConfigDockingWithShift = true;

		io.IniFilename = "imgui_editor.ini";

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		//ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)m_engine->getWindow()->getWindowHandle(), false);
		m_userInput = new UserInput(*m_engine->getWindow());
		m_imguiInputAdapter = new ImGuiInputAdapter(m_editorImGuiContext, *m_userInput, *m_engine->getWindow());

		ImGui::SetCurrentContext(prevImGuiContext);

		// register editor context with renderer
		// IMPORTANT: renderer expects main context to be current, so this needs to happen after switching back to the main context
		m_engine->getRenderSystem().initEditorImGuiCtx(m_editorImGuiContext);
	}

	uint32_t width = std::max(m_engine->getWidth(), 1u);
	uint32_t height = std::max(m_engine->getHeight(), 1u);

	auto &registry = m_engine->getEntityRegistry();
	m_editorCameraEntity = registry.create();
	registry.assign<TransformationComponent>(m_editorCameraEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(-12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f)));
	registry.assign<CameraComponent>(m_editorCameraEntity, CameraComponent::ControllerType::FPS, (width > 0 && height > 0) ? width / (float)height : 1.0f, glm::radians(60.0f), 0.1f, 300.0f);


	m_entityDetailWindow = new EntityDetailWindow(m_engine);
	m_entityDetailWindow->setVisible(true);
	m_entityWindow = new EntityWindow(m_engine);
	m_entityWindow->setVisible(true);
	m_assetBrowserWindow = new AssetBrowserWindow(m_engine);
	m_assetBrowserWindow->setVisible(true);

	m_gameLogic.initialize(engine);

	m_lastGameCameraEntity = m_engine->getRenderSystem().getCameraEntity();
	m_engine->setEditorMode(m_editorMode);
}

void VEditor::VEditor::update(float timeDelta)
{
	auto &renderSystem = m_engine->getRenderSystem();

	if (m_editorMode != m_newEditorMode)
	{
		m_editorMode = m_newEditorMode;
		m_engine->setEditorMode(m_editorMode);
		if (!m_editorMode)
		{
			renderSystem.setCameraEntity(m_lastGameCameraEntity);
		}
	}

	if (!m_editorMode && m_userInput->isKeyPressed(InputKey::ESCAPE))
	{
		m_newEditorMode = true;
	}

	if (m_editorMode)
	{
		auto window = m_engine->getWindow();
		//if (window->configurationChanged())
		{
			m_imguiInputAdapter->resize(window->getWidth(), window->getHeight(), window->getWindowWidth(), window->getWindowHeight());
		}

		m_userInput->input();
		m_imguiInputAdapter->update();


		// set editor imgui context
		auto *prevImGuiContext = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(m_editorImGuiContext);

		// ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		//ImGuizmo::BeginFrame();

		// begin main dock space/main menu bar
		{
			// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
			// because it would be confusing to have two docking targets within each others.
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			window_flags |= ImGuiWindowFlags_NoBackground;
			ImGuiViewport *viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
			// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
			// all active windows docked into it will lose their parent and become undocked.
			// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
			// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("MainDockSpace", nullptr, window_flags);
			ImGui::PopStyleVar();
			ImGui::PopStyleVar(2);

			// DockSpace
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpaceID");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
		}


		if (ImGui::BeginMainMenuBar())
		{
			//if (ImGui::BeginMenu("File"))
			//{
			//	// ShowExampleMenuFile();
			//	ImGui::EndMenu();
			//}
			//if (ImGui::BeginMenu("Edit"))
			//{
			//	if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			//	if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			//	ImGui::Separator();
			//	if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			//	if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			//	if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			//	ImGui::EndMenu();
			//}
			if (ImGui::BeginMenu("Window"))
			{
				//ImGui::MenuItem("Entities", "", &m_showEntityWindow);
				//ImGui::MenuItem("Details", "", &m_showEntityDetailWindow);
				//ImGui::MenuItem("Profiler", "", &m_showProfilerWindow);
				//ImGui::MenuItem("Memory", "", &m_showMemoryWindow);
				//ImGui::MenuItem("Exposure Settings", "", &m_showExposureWindow);
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		ImGui::End();

		// viewport
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin("Viewport");

			auto viewportOffset = ImGui::GetWindowContentRegionMin();
			auto windowPos = ImGui::GetWindowPos();
			viewportOffset.x += windowPos.x;
			viewportOffset.y += windowPos.y;
			auto viewportSize = ImGui::GetContentRegionAvail();

			ImGui::Image((ImTextureID)(size_t)renderSystem.getEditorSceneTextureHandle().m_handle, viewportSize);

			ImGui::End();
			ImGui::PopStyleVar();

			m_engine->setEditorViewport(viewportOffset.x, viewportOffset.y, std::max(viewportSize.x, 0.0f), std::max(viewportSize.y, 0.0f));

			// make sure aspect ratio of editor camera is correct
			m_engine->getEntityRegistry().get<CameraComponent>(m_editorCameraEntity).m_aspectRatio = (viewportSize.x > 0 && viewportSize.y > 0) ? viewportSize.x / (float)viewportSize.y : 1.0f;
		}

		if (m_entityWindow->isVisible())
		{
			m_entityWindow->draw();
		}

		if (m_entityDetailWindow->isVisible())
		{
			m_entityDetailWindow->draw(m_entityWindow->getSelectedEntity(), m_editorCameraEntity, prevImGuiContext);
		}

		// asset browser
		if (m_assetBrowserWindow->isVisible())
		{
			m_assetBrowserWindow->draw();
		}

		// modes
		{
			ImGui::Begin("Modes");

			ImGui::End();
		}

		// quick menu
		{
			ImGui::Begin("Quick Menu");

			if (ImGui::Button("Start"))
			{
				m_newEditorMode = false;
			}

			ImGui::End();
		}

		ImGui::Render();

		// restore context
		ImGui::SetCurrentContext(prevImGuiContext);

		renderSystem.setCameraEntity(m_lastGameCameraEntity);
	}

	m_gameLogic.update(timeDelta);

	if (m_editorMode)
	{
		// make sure we use the editor camera
		m_lastGameCameraEntity = renderSystem.setCameraEntity(m_editorCameraEntity);
	}
}

void VEditor::VEditor::shutdown()
{
	m_gameLogic.shutdown();
}
