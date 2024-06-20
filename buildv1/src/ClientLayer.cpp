#include "ClientLayer.h"


#include "Walnut/Application.h"
#include "Walnut/UI/UI.h"
#include "Walnut/Serialization/BufferStream.h"
#include "Walnut/Utils/StringUtils.h"

#include "misc/cpp/imgui_stdlib.h"

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

struct Funcs
{
    static int MyResizeCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            ImVector<char>* my_str = (ImVector<char>*)data->UserData;
            IM_ASSERT(my_str->begin() == data->Buf);
            my_str->resize(data->BufSize); // NB: On resizing calls, generally data->BufSize == data->BufTextLen + 1
            data->Buf = my_str->begin();
        }
        return 0;
    }

    // Note: Because ImGui:: is a namespace you would typically add your own function into the namespace.
    // For example, you code may declare a function 'ImGui::InputText(const char* label, MyString* my_str)'
    static bool MyInputTextMultiline(const char* label, std::string* my_str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        return ImGui::InputTextMultiline(label, my_str, size, flags, NULL, (void*)my_str);
    }
};

Walnut::Application* app;

void ClientLayer::SendApp(Walnut::Application* appx)
{
    app = appx;
}

struct FileDialog
{
    static std::string Open(const char* filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[_MAX_PATH] = { 0 };
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = glfwGetWin32Window(app->GetWindowHandle());
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            return ofn.lpstrFile;
        }
        return "-1";
    }

    static std::string Save(const char* filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[_MAX_PATH] = { 0 };
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = glfwGetWin32Window(app->GetWindowHandle());
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameA(&ofn) == TRUE)
        {
            return ofn.lpstrFile;
        }
        return "-1";
    }
};

int openFilesCounter = 0;

struct File
{
    std::string name = "New file " + std::to_string(openFilesCounter);

    std::string path;

    std::string text;

    bool open = true;
};

std::vector<File*> openFiles;

bool init = true;

File* currentFile;

void ClientLayer::OnUIRender()
{
    ImGuiID dockspace_id = ImGui::GetID("MyDockspace");

    if (demoWindowVisible) ImGui::ShowDemoWindow(&demoWindowVisible);

    if (init)
    {
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);

        ImGui::DockBuilderFinish(dockspace_id);

        init = false;
    }

    ///TODO: add unsaved file ball

    for (File* f : openFiles)
    {
        if (!f->open) 
        {
            openFiles.erase(std::remove(openFiles.begin(), openFiles.end(), f), openFiles.end());
            continue;
        }

        ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);

        bool active = ImGui::Begin(f->name.c_str(), &f->open, ImGuiWindowFlags_NoCollapse);

        if (f->text.empty())
            f->text.push_back(0);

        Funcs::MyInputTextMultiline("##MyStr", &f->text, ImVec2(-FLT_MIN, -FLT_MIN));

        ImGui::End();

        if (active)
            currentFile = f;
    }
}

void ClientLayer::newFile(std::string file)
{
    if (file == "-1")
        return;

    openFilesCounter++;
    File* brandNewFile = new File;
    if (file != "")
    {
        std::ifstream myfile(file);
        std::string line;

        brandNewFile->name = file.substr(file.find_last_of("\\") + 1);
        brandNewFile->path = file;

        while (std::getline(myfile, line)) {
            brandNewFile->text = brandNewFile->text + line + "\n";
        }
        myfile.close();
    }
    openFiles.push_back(brandNewFile);
}

void ClientLayer::openFile()
{
    newFile(FileDialog::Open(""));
}

void ClientLayer::saveCurrentFile(std::string file)
{
    if (file == "-1")
        return;
    
    if (currentFile != NULL)
    {
        if (file != "") {
            currentFile->name = file.substr(file.find_last_of("\\") + 1);
            currentFile->path = file;
        }

        if (currentFile->path == "")
        {
            saveFileAs();
            return;
        }

        std::ofstream outfile(currentFile->path);

        outfile << currentFile->text;

        outfile.close();

        std::cout << currentFile->text << std::endl;
    }
}

void ClientLayer::saveFileAs()
{
    if (currentFile != NULL)
    {
        saveCurrentFile(FileDialog::Save(""));
    }
}
