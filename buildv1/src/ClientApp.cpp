#include "Walnut/ApplicationGUI.h"
#include "Walnut/EntryPoint.h"

#include "ClientLayer.h"
#include <iostream>

const static uint64_t s_BufferSize = 1024;
static uint8_t* s_Buffer = new uint8_t[s_BufferSize];

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	ImGuiContext* context = ImGui::CreateContext();
	ImGui::SetCurrentContext(context);

	ImGui::GetIO().IniFilename = NULL;

	Walnut::ApplicationSpecification spec;
	spec.Name = "buildv1";
	spec.IconPath = "res/Walnut-Icon.png";
	spec.CustomTitlebar = true;
	spec.CenterWindow = true;

	Walnut::Application* app = new Walnut::Application(spec);
	std::shared_ptr<ClientLayer> clientLayer = std::make_shared<ClientLayer>();
	app->PushLayer(clientLayer);
	app->SetMenubarCallback([app, clientLayer]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New file", "Ctrl+N"))
			{
				clientLayer->newFile();
			}

			if (ImGui::MenuItem("Open...", "Ctrl+O"))
			{
				clientLayer->openFile();
			}

			if (ImGui::MenuItem("Save", "Ctrl+S"))
				clientLayer->saveCurrentFile();

			if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S"))
				clientLayer->saveFileAs();

			if (ImGui::MenuItem("Demo window"))
				clientLayer->demoWindowVisible = clientLayer->demoWindowVisible ? false : true;

			if (ImGui::MenuItem("Exit"))
				app->Close();
			ImGui::EndMenu();
		}
	});

	std::cout << "Have " << argc << " arguments:\n";
	for (int i = 0; i < argc; ++i) {
		std::cout << argv[i] << "\n";
		if (i >= 1)
		{
			clientLayer->newFile(argv[i]);
		}
	}

	clientLayer->SendApp(app);

	//ImGui::ShowDemoWindow();

	return app;
}