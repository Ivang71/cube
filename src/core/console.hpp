#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>

class Console {
public:
    using CommandCallback = std::function<void(const std::vector<std::string>& args)>;

    struct Command {
        std::string name;
        std::string description;
        CommandCallback callback;
    };

    struct Message {
        std::string text;
        std::chrono::steady_clock::time_point timestamp;
        bool is_chat; // true for chat messages, false for console output
    };

    Console();

    void register_command(const std::string& name, const std::string& description, CommandCallback callback);
    void execute_command(const std::string& input);
    void add_log_message(const std::string& message, bool is_chat = false);

    void render(bool* show_console);
    void render_chat_messages(); // Render messages on screen even when console is closed
    void set_focus();
    void set_input_text(const char* text, bool select_all = false);

    const std::vector<Message>& get_messages() const { return messages_; }
    std::vector<Message> get_recent_messages(float fade_time_seconds, size_t max_count) const;
    const std::unordered_map<std::string, Command>& get_commands() const { return commands_; }

private:
    static int input_callback(ImGuiInputTextCallbackData* data);
    void navigate_history(int direction);
    void trim_messages();

private:
    std::unordered_map<std::string, Command> commands_;
    std::vector<Message> messages_;
    std::vector<std::string> command_history_;
    char input_buffer_[1024];
    std::string current_input_; // Store current input when navigating history
    int history_index_;
    bool scroll_to_bottom_;
    bool should_focus_;

    static constexpr size_t MAX_MESSAGES = 1000;
    static constexpr size_t MAX_COMMAND_HISTORY = 100;

    void trim_log_messages();
    void trim_command_history();
};
