#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	struct AppEvent;
	
	class DebugFunctionStackUI : public UIView {
	public:
		using UIView::UIView;

		void Refresh();

		virtual void OnEvent(const AppEvent& e);
		virtual void Update(float delta);

	private:
		std::vector<std::string> m_stack;
	};
}