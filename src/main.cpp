// Vexdir 0.1.0 — C++17, FTXUI 7.0.3
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

struct Entry {
  fs::path path;
  std::string name;
  bool directory = false;
  std::uintmax_t size = 0;
  bool size_known = false;
};

std::string utf8(const fs::path& path) {
  return path.u8string();  // C++17: std::string, including on Windows
}

std::string lower_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::string human_size(std::uintmax_t bytes) {
  const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  if (unit == 0) return std::to_string(bytes) + " B";
  const auto scaled = static_cast<std::uintmax_t>(value * 10.0);
  return std::to_string(scaled / 10) + "." + std::to_string(scaled % 10) +
         " " + units[unit];
}

class Navigator {
 public:
  Navigator() {
    std::error_code ec;
    current_ = fs::current_path(ec);
    if (ec) {
      error_ = ec.message();
      current_ = fs::path(".");
    }
    refresh();
  }

  Element render() const {
    Elements lines;
    if (entries_.empty()) {
      lines.push_back(text("(empty directory)") | dim);
    } else {
      for (std::size_t index = 0; index < entries_.size(); ++index) {
        const Entry& entry = entries_[index];
        auto name = text(std::string(entry.directory ? "[DIR] " : "      ") +
                         entry.name) | flex;
        auto size = text(entry.directory ? "<DIR>" :
                         (entry.size_known ? human_size(entry.size) : "?"));
        auto line = hbox({name, text("  "), size});
        if (entry.directory) line = line | color(Color::Cyan);
        if (static_cast<int>(index) == selected_) {
          line = line | inverted | focus;
        }
        lines.push_back(line);
      }
    }

    auto content = vbox(std::move(lines)) | vscroll_indicator | frame | flex;
    auto header = hbox({
        text(" VEXDIR 0.1.0 ") | bold | color(Color::Cyan),
        filler(),
        text("READ ONLY ") | dim,
    });
    auto path_line = text(utf8(current_)) | color(Color::Yellow);
    auto status = text(error_.empty()
        ? std::to_string(entries_.size()) + " items"
        : "Error: " + error_);
    return vbox({
        header,
        separator(),
        path_line,
        separator(),
        content | border,
        status | dim,
        separator(),
        text(" Up/Down Navigate  Enter Open  Backspace Parent  F5 Refresh  Esc Exit ") |
            dim,
    }) | flex;
  }

  bool handle(const Event& event, ScreenInteractive& screen) {
    if (event == Event::Escape || event == Event::Character('q') ||
        event == Event::Character('Q')) {
      screen.Exit();
      return true;
    }
    if (event == Event::ArrowUp) return move(-1);
    if (event == Event::ArrowDown) return move(1);
    if (event == Event::PageUp) return move(-10);
    if (event == Event::PageDown) return move(10);
    if (event == Event::Home) return select(0);
    if (event == Event::End) return select(static_cast<int>(entries_.size()) - 1);
    if (event == Event::Return || event == Event::ArrowRight) {
      if (!entries_.empty() && selected_ >= 0 &&
          selected_ < static_cast<int>(entries_.size()) &&
          entries_[selected_].directory) {
        navigate(entries_[selected_].path);
      }
      return true;
    }
    if (event == Event::Backspace || event == Event::ArrowLeft) {
      navigate(current_.parent_path());
      return true;
    }
    if (event == Event::F5 || event == Event::Character('r') ||
        event == Event::Character('R')) {
      refresh();
      return true;
    }
    return false;
  }

 private:
  void refresh() {
    std::vector<Entry> next;
    std::error_code ec;
    fs::directory_iterator it(current_, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
      error_ = ec.message();
      return;
    }
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
      if (ec) break;
      Entry entry;
      entry.path = it->path();
      entry.name = utf8(entry.path.filename());
      std::error_code attr_ec;
      entry.directory = it->is_directory(attr_ec);
      if (!attr_ec && !entry.directory) {
        entry.size = it->file_size(attr_ec);
        entry.size_known = !attr_ec;
      }
      next.push_back(std::move(entry));
    }
    if (ec) {
      error_ = ec.message();
    } else {
      error_.clear();
    }
    std::sort(next.begin(), next.end(), [](const Entry& a, const Entry& b) {
      if (a.directory != b.directory) return a.directory;
      const auto x = lower_ascii(a.name);
      const auto y = lower_ascii(b.name);
      return x == y ? a.name < b.name : x < y;
    });
    entries_ = std::move(next);
    if (entries_.empty()) selected_ = 0;
    else selected_ = std::clamp(selected_, 0, static_cast<int>(entries_.size()) - 1);
  }

  void navigate(const fs::path& path) {
    if (path.empty() || path == current_) return;
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
      error_ = ec ? ec.message() : "Not a directory";
      return;
    }
    const auto old_path = current_;
    current_ = path;
    selected_ = 0;
    refresh();
    if (!error_.empty()) {
      current_ = old_path;
      refresh();
    }
  }

  bool select(int index) {
    if (entries_.empty()) return false;
    selected_ = std::clamp(index, 0, static_cast<int>(entries_.size()) - 1);
    return true;
  }

  bool move(int delta) { return select(selected_ + delta); }

  fs::path current_;
  std::vector<Entry> entries_;
  int selected_ = 0;
  std::string error_;
};

int main() {
  Navigator navigator;
  auto screen = ScreenInteractive::TerminalOutput();
  auto component = Renderer([&] { return navigator.render(); }) |
      CatchEvent([&](const Event& event) { return navigator.handle(event, screen); });
  screen.Loop(component);
  return 0;
}
