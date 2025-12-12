#include "console.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <chrono>

Console::Console()
    : history_index_(-1)
    , scroll_to_bottom_(false)
    , should_focus_(false)
{
    input_buffer_[0] = '\0';
}

void Console::register_command(const std::string& name, const std::string& description, CommandCallback callback) {
    commands_[name] = {name, description, callback};
}

void Console::execute_command(const std::string& input) {
    if (input.empty()) return;

    // Check if input starts with "/" (command) or is a chat message
    if (input[0] != '/') {
        command_history_.push_back(input);
        trim_command_history();
        history_index_ = -1;
        // This is a chat message - add it as a chat message and show temporarily
        add_log_message(input, true);
        return;
    }

    // This is a command - add to command history and log
    command_history_.push_back(input);
    trim_command_history();
    history_index_ = -1;

    // Add command to console log
    add_log_message("> " + input, false);

    // Remove the "/" prefix for parsing
    std::string command_input = input.substr(1);

    // Split command into tokens
    std::vector<std::string> args;
    std::string token;
    bool in_quotes = false;
    for (char c : command_input) {
        if (c == '"' || c == '\'') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!token.empty()) {
                args.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) {
        args.push_back(token);
    }

    if (args.empty()) {
        add_log_message("Unknown command. Type /help for available commands.", false);
        return;
    }

    std::string command_name = args[0];
    std::transform(command_name.begin(), command_name.end(), command_name.begin(), ::tolower);

    auto it = commands_.find(command_name);
    if (it != commands_.end()) {
        try {
            it->second.callback(args);
        } catch (const std::exception& e) {
            add_log_message("Error executing command '" + command_name + "': " + e.what(), false);
        }
    } else {
        add_log_message("Unknown command: /" + command_name, false);
    }
}

void Console::add_log_message(const std::string& message, bool is_chat) {
    messages_.push_back({message, std::chrono::steady_clock::now(), is_chat});
    trim_messages();
    scroll_to_bottom_ = true;
}

void Console::render(bool* show_console) {
    if (!*show_console) return;

    // Get display size
    ImVec2 display_size = ImGui::GetIO().DisplaySize;

    // Constants
    const float margin_x = 20.0f;
    const float bottom_margin = 20.0f;
    const float gap = 60.0f;
    const float scale = 1.9f;
    const float input_pad_y = 1.0f;

    const float line_h = (ImGui::GetFontSize() * scale) + 4.0f;
    const float input_h = (ImGui::GetFontSize() * scale) + (input_pad_y * 2.0f) + 10.0f;
    const float max_output_h = display_size.y * 0.75f;

    const bool has_output = !messages_.empty();
    const float output_h = has_output ? std::min(max_output_h, (line_h * static_cast<float>(messages_.size()))) : 0.0f;

    const float input_top_y = display_size.y - bottom_margin - input_h;
    const float chat_bottom_y = input_top_y - gap;
    const float output_top_y = chat_bottom_y - output_h;
    const float total_h = input_h + (has_output ? (gap + output_h) : 0.0f);

    // Calculate content width (same for both input and output)
    const float content_width = display_size.x - 2 * margin_x;
    float console_y = has_output ? output_top_y : input_top_y;

    // Position console to contain both input and output
    ImGui::SetNextWindowPos(ImVec2(margin_x, console_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(content_width, total_h), ImGuiCond_Always);

    // Style for console - no background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent background
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); // No border
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); // No padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f); // No rounding

    if (ImGui::Begin("Console", show_console,
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar)) {

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float w = ImGui::GetWindowWidth();
        ImU32 out_bg = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
        ImU32 in_bg = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f));

        float input_y = 0.0f;
        if (has_output) {
            dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + w, wp.y + output_h), out_bg);
            input_y = output_h + gap;
        }
        dl->AddRectFilled(ImVec2(wp.x, wp.y + input_y),
                          ImVec2(wp.x + w, wp.y + input_y + input_h), in_bg);

        if (has_output) {
            ImGui::SetWindowFontScale(scale);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::BeginChild("Output", ImVec2(0, output_h), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
            for (const auto& msg : messages_) ImGui::TextUnformatted(msg.text.c_str());
            if (scroll_to_bottom_) {
                ImGui::SetScrollHereY(1.0f);
                scroll_to_bottom_ = false;
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(1);
            ImGui::Dummy(ImVec2(0.0f, gap));
        }

        // Input area - always at bottom, same width as output
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImVec2 fp = ImGui::GetStyle().FramePadding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(fp.x, input_pad_y));
        ImGui::SetWindowFontScale(scale);

        // Handle input - full width like output
        if (should_focus_) {
            ImGui::SetKeyboardFocusHere();
            should_focus_ = false;
        }
        bool enter_pressed = false;
        ImGui::PushItemWidth(-1.0f); // Full width
        if (ImGui::InputText("##input", input_buffer_, sizeof(input_buffer_),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
                            &Console::input_callback, this)) {
            enter_pressed = true;
        }

        // Pop input styling
        ImGui::PopItemWidth();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(1);

        if (enter_pressed) {
            if (input_buffer_[0] != '\0') {
                execute_command(input_buffer_);
                input_buffer_[0] = '\0';
            }
            *show_console = false;
        }
    }

    ImGui::End();

    // Pop the style changes
    ImGui::PopStyleVar(3); // WindowBorderSize, WindowPadding, WindowRounding
    ImGui::PopStyleColor(1); // WindowBg
}

void Console::set_focus() {
    should_focus_ = true;
}

void Console::set_input_text(const char* text, bool select_all) {
    if (text) {
        strcpy(input_buffer_, text);
    } else {
        input_buffer_[0] = '\0';
    }
    should_focus_ = true;
}

void Console::render_chat_messages() {
    auto now = std::chrono::steady_clock::now();
    constexpr float hold_seconds = 5.0f;
    constexpr float fade_seconds = 1.0f;
    constexpr int max_lines = 10;
    std::vector<const Message*> lines;
    lines.reserve(max_lines);
    for (auto it = messages_.rbegin(); it != messages_.rend() && static_cast<int>(lines.size()) < max_lines; ++it) {
        lines.push_back(&(*it));
    }
    if (lines.empty()) return;
    std::reverse(lines.begin(), lines.end());

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const float margin_x = 20.0f;
    const float bottom_margin = 20.0f;
    const float gap = 60.0f;
    const float font_scale = 1.9f;
    const float input_pad_y = 1.0f;
    const float input_h = (ImGui::GetFontSize() * font_scale) + (input_pad_y * 2.0f) + 10.0f;
    const float line_height = (ImGui::GetFontSize() * font_scale);
    const float pad_y = 0.0f;
    const float pad_x = 0.0f;
    const float msg_pad_y = 1.0f;
    const float slot_h = line_height + msg_pad_y * 2.0f;
    const float window_h = slot_h * static_cast<float>(max_lines);
    const float window_w = display_size.x - 2 * margin_x;
    const float chat_bottom_y = (display_size.y - bottom_margin - input_h) - gap;
    ImGui::SetNextWindowPos(ImVec2(margin_x, chat_bottom_y - window_h), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_w, window_h), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); // No border
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad_x, pad_y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f); // No rounding
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("Chat", nullptr,
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoFocusOnAppearing)) {

        ImGui::SetWindowFontScale(font_scale);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 win_pos = ImGui::GetWindowPos();
        for (int i = 0; i < max_lines; ++i) {
            int src = static_cast<int>(lines.size()) - 1 - i;
            if (src < 0) break;
            const Message& msg = *lines[static_cast<size_t>(src)];
            float age = std::chrono::duration_cast<std::chrono::duration<float>>(now - msg.timestamp).count();
            float a = 1.0f;
            if (age > hold_seconds) {
                float u = (age - hold_seconds) / fade_seconds;
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;
                a = 1.0f - u;
            }
            if (a <= 0.0f) continue;

            float y = window_h - (static_cast<float>(i + 1) * slot_h);
            ImVec2 p0(win_pos.x, win_pos.y + y);
            ImVec2 p1(win_pos.x + window_w, win_pos.y + y + slot_h);
            ImU32 bg = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f * a));
            dl->AddRectFilled(p0, p1, bg);

            ImVec4 col(1.0f, 1.0f, 1.0f, a);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::SetCursorPos(ImVec2(0.0f, y + msg_pad_y));
            ImGui::TextUnformatted(msg.text.c_str());
            ImGui::PopStyleColor(1);
        }
    }

    ImGui::End();

    // Pop the style changes
    ImGui::PopStyleVar(4); // WindowBorderSize, WindowPadding, WindowRounding, ItemSpacing
    ImGui::PopStyleColor(1); // WindowBg
}

int Console::input_callback(ImGuiInputTextCallbackData* data) {
    Console* console = static_cast<Console*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (data->EventKey == ImGuiKey_UpArrow) console->navigate_history(-1);
        if (data->EventKey == ImGuiKey_DownArrow) console->navigate_history(1);
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, console->input_buffer_);
    }
    return 0;
}

void Console::navigate_history(int direction) {
    if (command_history_.empty()) return;

    if (history_index_ == -1) {
        // First time navigating, save current input
        current_input_ = input_buffer_;
        if (direction >= 0) return;
        history_index_ = static_cast<int>(command_history_.size()) - 1;
        strcpy(input_buffer_, command_history_[history_index_].c_str());
        return;
    }

    int new_index = history_index_ + direction;
    if (new_index < 0) new_index = 0;
    if (new_index >= static_cast<int>(command_history_.size())) {
        history_index_ = -1;
        strcpy(input_buffer_, current_input_.c_str());
        return;
    }

    history_index_ = new_index;
    strcpy(input_buffer_, command_history_[history_index_].c_str());
}

void Console::trim_messages() {
    if (messages_.size() > MAX_MESSAGES) {
        messages_.erase(messages_.begin(),
                       messages_.begin() + (messages_.size() - MAX_MESSAGES));
    }
}

std::vector<Console::Message> Console::get_recent_messages(float fade_time_seconds, size_t max_count) const {
    auto now = std::chrono::steady_clock::now();
    std::vector<Message> out;
    out.reserve(max_count);
    for (auto it = messages_.rbegin(); it != messages_.rend(); ++it) {
        auto age = std::chrono::duration_cast<std::chrono::duration<float>>(now - it->timestamp).count();
        if (age > fade_time_seconds) break;
        out.push_back(*it);
        if (out.size() >= max_count) break;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

void Console::trim_command_history() {
    if (command_history_.size() > MAX_COMMAND_HISTORY) {
        command_history_.erase(command_history_.begin(),
                              command_history_.begin() + (command_history_.size() - MAX_COMMAND_HISTORY));
    }
}
