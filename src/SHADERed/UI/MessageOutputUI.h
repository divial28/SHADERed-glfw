#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	struct AppEvent;
	
	class MessageOutputUI : public UIView {
	public:
		using UIView::UIView;

		virtual void OnEvent(const AppEvent& e);
		virtual void Update(float delta);
	};
}