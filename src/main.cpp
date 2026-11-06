// Vexdir 0.1.2 — C++17; no third-party libraries.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string utf8(const fs::path& p) { return p.u8string(); }

struct Entry {
  fs::path path;
  std::string name;
  bool directory = false;
  std::uintmax_t bytes = 0;
  bool known = false;
};

static std::string fold(std::string s) {
  for (auto& c : s) {
    auto b = static_cast<unsigned char>(c);
    if (b >= 'A' && b <= 'Z') c = static_cast<char>(b + ('a' - 'A'));
  }
  return s;
}

static std::string size_label(std::uintmax_t n) {
  const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double v = static_cast<double>(n);
  int i = 0;
  while (v >= 1024 && i < 4) { v /= 1024; ++i; }
  if (i == 0) return std::to_string(n) + " B";
  auto tenths = static_cast<unsigned long long>(v * 10);
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10)
      + " " + units[i];
}

// Trim a UTF-8 string without splitting a multibyte character.
static std::string truncate_utf8(const std::string& s, int max_columns) {
  if (max_columns <= 0) return {};
  std::string out;
  int columns = 0;
  for (std::size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    std::size_t n = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
    if (i + n > s.size() || columns >= max_columns) break;
    out.append(s, i, n);
    i += n;
    ++columns;
  }
  return out;
}

enum class Key { None, Up, Down, Left, Right, Home, End, PageUp,
                 PageDown, Enter, Back, Refresh, Exit, Resize };

class Terminal {
 public:
  Terminal() {
#ifdef _WIN32
    input_ = GetStdHandle(STD_INPUT_HANDLE);
    output_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input_ == INVALID_HANDLE_VALUE || output_ == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(input_, &old_in_) ||
        !GetConsoleMode(output_, &old_out_)) return;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    const DWORD mode = (old_in_ & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
        ENABLE_QUICK_EDIT_MODE)) | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
    if (!SetConsoleMode(input_, mode)) return;
    if (!SetConsoleMode(output_, old_out_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
      SetConsoleMode(input_, old_in_);
      return;
    }
#else
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &old_term_) != 0) return;
    termios raw = old_term_;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return;
#endif
    ready_ = true;
    std::cout << "\x1b[?1049h\x1b[?25l\x1b[?7l" << std::flush;
  }

  ~Terminal() {
    if (!ready_) return;
    std::cout << "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l" << std::flush;
#ifdef _WIN32
    SetConsoleMode(input_, old_in_);
    SetConsoleMode(output_, old_out_);
#else
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term_);
#endif
  }

  bool ready() const { return ready_; }

  std::pair<int, int> dimensions() const {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(output_, &info)) {
      return {info.srWindow.Right - info.srWindow.Left + 1,
              info.srWindow.Bottom - info.srWindow.Top + 1};
    }
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row)
      return {ws.ws_col, ws.ws_row};
#endif
    return {80, 24};
  }

  Key read_key() {
#ifdef _WIN32
    INPUT_RECORD record{};
    DWORD count = 0;
    while (ReadConsoleInputW(input_, &record, 1, &count)) {
      if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) return Key::Resize;
      if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
      const auto& e = record.Event.KeyEvent;
      switch (e.wVirtualKeyCode) {
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_RETURN: return Key::Enter;
        case VK_BACK: return Key::Back;
        case VK_F5: return Key::Refresh;
        case VK_ESCAPE: return Key::Exit;
      }
      if (e.uChar.UnicodeChar == L'q' || e.uChar.UnicodeChar == L'Q') return Key::Exit;
      if (e.uChar.UnicodeChar == L'r' || e.uChar.UnicodeChar == L'R') return Key::Refresh;
    }
    return Key::Exit;
#else
    unsigned char c = 0;
    if (!read_byte(c, -1)) return Key::Resize;
    if (c == '\r' || c == '\n') return Key::Enter;
    if (c == 127 || c == 8) return Key::Back;
    if (c == 'q' || c == 'Q') return Key::Exit;
    if (c == 'r' || c == 'R') return Key::Refresh;
    if (c != 27) return Key::None;
    if (!read_byte(c, 40)) return Key::Exit;
    if (c != '[' && c != 'O') return Key::None;
    if (!read_byte(c, 40)) return Key::None;
    switch (c) {
      case 'A': return Key::Up;
      case 'B': return Key::Down;
      case 'C': return Key::Right;
      case 'D': return Key::Left;
      case 'H': return Key::Home;
      case 'F': return Key::End;
    }
    if (c >= '0' && c <= '9') {
      int number = c - '0';
      while (read_byte(c, 40)) {
        if (c == '~') {
          switch (number) {
            case 1: case 7: return Key::Home;
            case 4: case 8: return Key::End;
            case 5: return Key::PageUp;
            case 6: return Key::PageDown;
            case 15: return Key::Refresh;
          }
          break;
        }
        if (c < '0' || c > '9') break;
        number = number * 10 + (c - '0');
      }
    }
    return Key::None;
#endif
  }

 private:
#ifndef _WIN32
  bool read_byte(unsigned char& c, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    const int result = select(STDIN_FILENO + 1, &fds, nullptr, nullptr,
                              timeout_ms < 0 ? nullptr : &tv);
    return result > 0 && ::read(STDIN_FILENO, &c, 1) == 1;
  }
  termios old_term_{};
#else
  HANDLE input_ = INVALID_HANDLE_VALUE;
  HANDLE output_ = INVALID_HANDLE_VALUE;
  DWORD old_in_ = 0;
  DWORD old_out_ = 0;
#endif
  bool ready_ = false;
};

class Navigator {
 public:
  Navigator() {
    std::error_code ec;
    current_ = fs::current_path(ec);
    if (ec) current_ = ".";
    refresh();
  }

  void refresh() {
    std::vector<Entry> next;
    std::error_code ec;
    fs::directory_iterator it(current_, fs::directory_options::skip_permission_denied, ec);
    if (ec) { error_ = ec.message(); return; }
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
      if (ec) break;
      Entry e;
      e.path = it->path();
      e.name = utf8(e.path.filename());
      std::error_code attribute_error;
      e.directory = it->is_directory(attribute_error);
      if (!e.directory && !attribute_error) {
        e.bytes = it->file_size(attribute_error);
        e.known = !attribute_error;
      }
      next.push_back(std::move(e));
    }
    if (ec) { error_ = ec.message(); return; }
    std::sort(next.begin(), next.end(), [](const Entry& a, const Entry& b) {
      if (a.directory != b.directory) return a.directory;
      const auto left = fold(a.name), right = fold(b.name);
      return left == right ? a.name < b.name : left < right;
    });
    entries_ = std::move(next);
    if (entries_.empty()) selected_ = 0;
    else selected_ = std::clamp(selected_, 0, static_cast<int>(entries_.size()) - 1);
    error_.clear();
  }

  bool handle(Key key, int page) {
    const int last = static_cast<int>(entries_.size()) - 1;
    switch (key) {
      case Key::Exit: return false;
      case Key::Up: selected_ = std::max(0, selected_ - 1); break;
      case Key::Down: selected_ = std::min(std::max(0, last), selected_ + 1); break;
      case Key::PageUp: selected_ = std::max(0, selected_ - page); break;
      case Key::PageDown: selected_ = std::min(std::max(0, last), selected_ + page); break;
      case Key::Home: selected_ = 0; break;
      case Key::End: selected_ = std::max(0, last); break;
      case Key::Back: case Key::Left: navigate(current_.parent_path()); break;
      case Key::Enter: case Key::Right:
        if (selected_ <= last && !entries_.empty() && entries_[selected_].directory)
          navigate(entries_[selected_].path);
        break;
      case Key::Refresh: refresh(); break;
      default: break;
    }
    return true;
  }

  std::vector<std::string> frame(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    const int visible = std::max(0, height - 6);
    if (selected_ < offset_) offset_ = selected_;
    if (visible > 0 && selected_ >= offset_ + visible)
      offset_ = selected_ - visible + 1;
    offset_ = std::max(0, std::min(offset_,
       std::max(0, static_cast<int>(entries_.size()) - visible)));
    std::vector<std::string> rows;
    rows.reserve(static_cast<std::size_t>(height));
    auto add = [&](const std::string& color, const std::string& content) {
      auto line = truncate_utf8(content, width);
      int padding = std::max(0, width - static_cast<int>(utf8_columns(line)));
      rows.push_back(color + line + std::string(static_cast<std::size_t>(padding), ' ') +
                     "\x1b[0m");
    };
    add("\x1b[1;97;44m", " VEXDIR 0.1.2  |  FILE NAVIGATOR  |  READ ONLY");
    add("\x1b[1;93;44m", " " + utf8(current_));
    add("\x1b[1;96;44m", " NAME" +
        std::string(static_cast<std::size_t>(std::max(1, width - 13)), ' ') +
        "SIZE");
    for (int row = 0; row < visible; ++row) {
      const int index = offset_ + row;
      if (index >= static_cast<int>(entries_.size())) {
        add("\x1b[37;44m", "");
        continue;
      }
      const auto& e = entries_[index];
      const int name_width = std::max(1, width - 17);
      auto label = truncate_utf8(
          std::string(e.directory ? " [DIR] " : "       ") + e.name, name_width);
      label += std::string(static_cast<std::size_t>(std::max(0,
          name_width - static_cast<int>(utf8_columns(label)))), ' ');
      auto size = e.directory ? "<DIR>" : e.known ? size_label(e.bytes) : "?";
      const auto content = label + " " + size;
      add(index == selected_ ? "\x1b[1;30;106m" :
          e.directory ? "\x1b[1;96;44m" : "\x1b[97;44m", content);
    }
    add(error_.empty() ? "\x1b[1;92;44m" : "\x1b[1;91;44m",
        error_.empty() ? " " + std::to_string(entries_.size()) + " items" :
                         " Error: " + error_);
    add("\x1b[1;30;46m",
        " Arrows: Move  Enter: Open  Backspace: Up  F5: Refresh  Esc: Exit");
    add("\x1b[37;44m", "");
    return rows;
  }

 private:
  static std::size_t utf8_columns(const std::string& s) {
    std::size_t count = 0;
    for (unsigned char c : s) if ((c & 0xc0) != 0x80) ++count;
    return count;
  }
  static void line(const std::string& s, int width) {
    std::cout << truncate_line_with_escape(s, width) << "\x1b[K\n";
  }
  static std::string truncate_line_with_escape(const std::string& s, int width) {
    // Styled headers are fixed-width. Paths and error text are already shortened.
    (void)width;
    return s;
  }
  void navigate(const fs::path& next) {
    if (next.empty() || next == current_) return;
    std::error_code ec;
    if (!fs::is_directory(next, ec)) {
      error_ = ec ? ec.message() : "Not a directory";
      return;
    }
    auto previous = current_;
    auto previous_entries = entries_;
    int previous_selection = selected_;
    int previous_offset = offset_;
    current_ = next;
    selected_ = offset_ = 0;
    refresh();
    if (!error_.empty()) {
      current_ = std::move(previous);
      entries_ = std::move(previous_entries);
      selected_ = previous_selection;
      offset_ = previous_offset;
    }
  }
  fs::path current_;
  std::vector<Entry> entries_;
  int selected_ = 0;
  int offset_ = 0;
  std::string error_;
};

// Per-row VT updates avoid clearing the entire console on every key press.
class Painter {
 public:
  explicit Painter(Terminal& terminal) : terminal_(terminal) {}
  void paint(const std::vector<std::string>& rows, int width) {
    if (width != previous_width_ || previous_.size() != rows.size())
      previous_.clear();
    std::string output;
    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (i < previous_.size() && rows[i] == previous_[i]) continue;
      output += "\x1b[" + std::to_string(i + 1) + ";1H" + rows[i];
    }
    if (!output.empty()) std::cout << output << std::flush;
    previous_ = rows;
    previous_width_ = width;
  }
 private:
  Terminal& terminal_;
  std::vector<std::string> previous_;
  int previous_width_ = 0;
};

int main() {
  Terminal terminal;
  if (!terminal.ready()) {
    std::cerr << "Vexdir requires an interactive terminal with ANSI/VT support.\n";
    return 1;
  }
  Navigator navigator;
  Painter painter(terminal);
  while (true) {
    auto [width, height] = terminal.dimensions();
    painter.paint(navigator.frame(width, height), width);
    if (!navigator.handle(terminal.read_key(), std::max(1, height - 6))) break;
  }
  return 0;
}
