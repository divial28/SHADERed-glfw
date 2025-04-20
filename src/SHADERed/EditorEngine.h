#pragma once
#include <SHADERed/GUIManager.h>
#include <SHADERed/InterfaceManager.h>

struct GLFWwindow;

namespace ed {
	struct AppEvent;

	class EditorEngine {
	public:
		EditorEngine(GLFWwindow* wnd);

		void OnEvent(const AppEvent& e);

		void Update(float delta);
		void Render();

		void Create();
		void Destroy();

		inline InterfaceManager& Interface() { return m_interface; }
		inline GUIManager& UI() { return m_ui; }

	private:
		InterfaceManager m_interface;
		GUIManager m_ui;
	};
}