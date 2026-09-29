#include "Console.h"

namespace console {
namespace {

struct Command {
  const char *name;
  const char *help;
  Handler handler;
};

constexpr size_t kMaxCommands = 48;
Command commands[kMaxCommands];
size_t commandCount = 0;
String line;

void dispatch(String input) {
  input.trim();
  if (input.isEmpty()) return;
  String name = nextWord(input);
  if (name == "help" || name == "?") {
    printHelp();
    return;
  }
  for (size_t i = 0; i < commandCount; i++) {
    if (name.equalsIgnoreCase(commands[i].name)) {
      commands[i].handler(input);
      return;
    }
  }
  Serial.printf("Unknown command '%s'. Type 'help'.\n", name.c_str());
}

}  // namespace

void add(const char *name, const char *help, Handler handler) {
  if (commandCount < kMaxCommands) commands[commandCount++] = {name, help, handler};
}

void printHelp() {
  Serial.println("Commands:");
  for (size_t i = 0; i < commandCount; i++) {
    Serial.printf("  %-12s %s\n", commands[i].name, commands[i].help);
  }
}

void poll() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      if (!line.isEmpty()) {
        String input = line;
        line = "";
        dispatch(input);
      }
    } else if (line.length() < 512) {
      line += c;
    }
  }
}

String nextWord(String &args) {
  args.trim();
  int space = args.indexOf(' ');
  String word = space < 0 ? args : args.substring(0, space);
  args = space < 0 ? "" : args.substring(space + 1);
  args.trim();
  return word;
}

}  // namespace console
