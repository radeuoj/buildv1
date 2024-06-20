#pragma once

#include "Walnut/Layer.h"
#include "Walnut/ApplicationGUI.h"

#include "Walnut/UI/Console.h"


#include <set>
#include <filesystem>

class ClientLayer : public Walnut::Layer
{
public:
	void SendApp(Walnut::Application* appx);
	virtual void OnUIRender() override;
	void newFile(std::string file = "");
	void openFile();
	void saveFileAs();
	void saveCurrentFile(std::string file = "");
	bool demoWindowVisible = false;
private:
	Walnut::UI::Console m_Console{ "Chat" };
};