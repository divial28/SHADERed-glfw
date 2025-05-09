#pragma once
#include <SHADERed/UI/UIView.h>

namespace ed {
	struct AppEvent;

	class FrameAnalysisUI : public UIView {
	public:
		FrameAnalysisUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{ 
			m_histogramType = 0;
		}
		~FrameAnalysisUI() { }

		void Process();

		virtual void OnEvent(const AppEvent& e);
		virtual void Update(float delta);

	private:
		int m_histogramType;
		float m_histogramR[256], m_histogramG[256], m_histogramB[256], m_histogramRGB[256];

		std::vector<float> m_pixelHeights;
	};
}