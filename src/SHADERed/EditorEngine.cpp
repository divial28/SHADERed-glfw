#include <SHADERed/EditorEngine.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/SystemVariableManager.h>

namespace ed {
	EditorEngine::EditorEngine(GLFWwindow* wnd)
			: m_ui(&m_interface, wnd)
			, m_interface(&m_ui)
	{
	}
	void EditorEngine::Create()
	{
		m_ui.LoadSettings();

		// load template
		m_interface.Pipeline.New();
	}

	void EditorEngine::OnEvent(const AppEvent& e)
	{
		m_ui.OnEvent(e);
		m_interface.OnEvent(e);
	}

	void EditorEngine::Update(float delta)
	{
		// first update system time delta value
		SystemVariableManager::Instance().SetTimeDelta(delta);

		m_ui.Update(delta);
		m_interface.Update(delta);
	}
	void EditorEngine::Render()
	{
		m_ui.Render();
	}
	void EditorEngine::Destroy()
	{
		m_interface.DAP.Terminate();
		m_interface.Pipeline.Clear();
		m_ui.SaveSettings();
		Settings::Instance().Save();
	}
}