#include "ClientLayer.h"


#include "Walnut/Application.h"
#include "Walnut/UI/UI.h"
#include "Walnut/Serialization/BufferStream.h"
#include "Walnut/Utils/StringUtils.h"

#include "misc/cpp/imgui_stdlib.h"
#include <imgui_internal.h>

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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

void syntaxHighlight(File* f)
{

    constexpr int COLORED_ELEMENTS = 2;
    static char colored_token_prefix[COLORED_ELEMENTS][64] = {
        "kit",
        "pamoli"
    };

    struct Token { int begin; int end; };

    struct InputTextUserData { ImVector<Token> tokens; ImVec4 color = ImVec4(255.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f); };
    static InputTextUserData textData;

    struct SyntaxHighlight
    {
        static void TokenizeStr(ImVector<Token>& my_tokens, const char* buf)
        {
            if (!my_tokens.empty())
                return;
            int token_begin = -1;
            const int text_length = (int)strlen(buf);
            for (int i = 0; i < text_length; ++i)
            {
                const char c = buf[i];
                if (c == ' ' || c == '\t' || c == '\n')
                {
                    if (token_begin != -1)
                    {
                        Token token;
                        token.begin = token_begin;
                        token.end = i;
                        my_tokens.push_back(token);
                        token_begin = -1;
                    }
                    continue;
                }
                if (token_begin == -1)
                    token_begin = i;
            }
            if (token_begin != -1)
            {
                Token token;
                token.begin = token_begin;
                token.end = text_length;
                my_tokens.push_back(token);
            }
            return;
        }

        static std::string GetWord(const char* text)
        {
            std::string newText(text);
            int find = newText.find(" ");
            std::string res = newText.substr(0, find);
            if (res == "")
                return newText;
            return res;
        }

        static int Strnicmp(const char* str1, const char* str2, int n) 
        {
            int d = 0; 
            while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) 
            { 
                str1++; 
                str2++; 
                n--; 
            }
            return d; 
        }

        static int MyInputTextCallback(ImGuiInputTextCallbackData* callback_data)
        {
            InputTextUserData* user_data = (InputTextUserData*)callback_data->UserData;
            if (callback_data->EventFlag == ImGuiInputTextFlags_CallbackColor)
            {
                ImGuiTextColorCallbackData* color_data = callback_data->ColorData;
                const int char_idx = color_data->Char - color_data->TextBegin;
                while (color_data->TokenIdx < user_data->tokens.size() && char_idx >= user_data->tokens[color_data->TokenIdx].end) // jump to the first token that does not end before the current char index (could binary search here)
                    ++color_data->TokenIdx;
                if (color_data->TokenIdx >= user_data->tokens.size()) // all tokens end before the current char index
                {
                    color_data->CharsUntilCallback = 0; // stop calling back
                    return 0;
                }
                if (char_idx < user_data->tokens[color_data->TokenIdx].begin)
                {
                    color_data->CharsUntilCallback = user_data->tokens[color_data->TokenIdx].begin - char_idx; // wait until we reach the first token
                    return 0;
                }

                std::string word = GetWord(&color_data->TextBegin[user_data->tokens[color_data->TokenIdx].begin]);

                bool exists = std::find(std::begin(colored_token_prefix), std::end(colored_token_prefix), word) != std::end(colored_token_prefix);

                //std::cout << exists << std::endl;

                //for (int i = 0; i < COLORED_ELEMENTS; i++)
                {
                    // If the current token matches the prefix, color it red
                    if (exists)
                    {
                        color_data->Color = ImColor(user_data->color);
                        color_data->CharsForColor = word.size()/*user_data->tokens[color_data->TokenIdx].end - char_idx*/; // color from the current char to the token end
                    }
                }

                // If there is another token, callback once we hit the start of it. otherwise, stop calling back.
                color_data->CharsUntilCallback = (color_data->TokenIdx + 1 < user_data->tokens.size()) ? user_data->tokens[color_data->TokenIdx + 1].begin - char_idx : 0;
            }
            else if (callback_data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
                user_data->tokens.clear();
            else
                TokenizeStr(user_data->tokens, callback_data->Buf);
            return 0;
        }
    };

    static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter;
    if (colored_token_prefix[0] != '\0') { flags |= ImGuiInputTextFlags_CallbackColor; }
    else { flags &= ~ImGuiInputTextFlags_CallbackColor; }
    if (ImGui::InputTextMultiline("##MyStr", &f->text, ImVec2(-FLT_MIN, -FLT_MIN), flags, SyntaxHighlight::MyInputTextCallback, &textData))
        textData.tokens.clear();
    if (ImGui::IsItemDeactivated()) { textData.tokens.clear(); } // deactivated, trigger re-tokenization in case the edits were reverted
    if (!ImGui::IsItemActive()) { SyntaxHighlight::TokenizeStr(textData.tokens, f->text.c_str()); } // inactive, try to tokenize because it may have never been activated
}

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

        syntaxHighlight(f);

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
