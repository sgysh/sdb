/****************************************************************************
 * sdb.cc
 *
 *   Copyright (c) 2017 Yoshinori Sugino
 *   This software is released under the MIT License.
 ****************************************************************************/
#ifndef __x86_64__
#error "not supported"
#endif

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <alloca.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

namespace {

std::vector<std::string> splitString(const std::string &input, char delimiter) {
  std::istringstream       stream(input);
  std::string              substring;
  std::vector<std::string> result;

  while (std::getline(stream, substring, delimiter)) {
    if (substring.empty()) continue;
    result.push_back(substring);
  }

  return result;
}

unsigned long long int getPC(pid_t pid) {
  struct user_regs_struct regs;

  (void)ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
  return regs.rip;
}

void decrementPC(pid_t pid) {
  struct user_regs_struct regs;

  (void)ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
  regs.rip -= 1;
  (void)ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
}

void waitForProcess(pid_t pid) {
  int status;

  (void)waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    std::cout << "Program exited normally (process " << pid << ")" << std::endl;
    exit(0);
  }
  if (WIFSTOPPED(status)) {
    std::cout << "stop" << std::endl;
  }
}

/*
 * Example
 *
 * # original
 * OFFSET    01
 * OFFSET+1  fc
 * OFFSET+2  45
 * OFFSET+3  83
 *
 * # breakpoint inserted
 * OFFSET    cc
 * OFFSET+1  fc
 * OFFSET+2  45
 * OFFSET+3  83
 *
 * # after breakpoint
 * OFFSET    cc
 * OFFSET+1  fc   <---- PC points here
 * OFFSET+2  45
 * OFFSET+3  83
 *
 * After the CPU executes the INT3 instruction, the PC gets incremented by one.
 */

struct Breakpoint {
  bool      set;
  uintptr_t bp_addr;
  uintptr_t rw_addr;
  long      save_text;

  Breakpoint ()
    : set(false),
      bp_addr(),
      rw_addr(),
      save_text() {}
};

Breakpoint breakProcess(pid_t pid, uintptr_t addr) {
  Breakpoint bp;

  bp.bp_addr = addr;
  bp.rw_addr = bp.bp_addr & ~(sizeof(long) - 1);
  bp.save_text = ptrace(PTRACE_PEEKTEXT, pid, (void *)bp.rw_addr, nullptr);

  // 0xCC is the opcode for INT3
  auto shift = 8 * (bp.bp_addr - bp.rw_addr);
  long modified_text = (bp.save_text & ~((long)0xFF << shift)) | (long)0xCC << shift;

  (void)ptrace(PTRACE_POKETEXT, pid, (void *)bp.rw_addr, (void *)modified_text);

  bp.set = true;

  return bp;
}

void deleteBreakPoint(pid_t pid, const Breakpoint &bp) {
  (void)ptrace(PTRACE_POKETEXT, pid, (void *)bp.rw_addr, (void *)bp.save_text);
}

void attachProcess(pid_t pid) {
  (void)ptrace(PTRACE_ATTACH, pid, nullptr, nullptr);
}

void detachProcess(pid_t pid) {
  (void)ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
}

void continueProcess(pid_t pid, const Breakpoint &bp) {
  std::cout << "Continuing" << std::endl;

  (void)ptrace(PTRACE_CONT, pid, nullptr, nullptr);
  waitForProcess(pid);

  if (bp.set && bp.bp_addr == getPC(pid) - 1) {
    std::cout << "Breakpoint at 0x" << std::hex << getPC(pid) - 1 << std::dec << std::endl;
    deleteBreakPoint(pid, bp);
    decrementPC(pid);
  }
}

void stepProcess(pid_t pid) {
  (void)ptrace(PTRACE_SINGLESTEP, pid, nullptr, nullptr);
  waitForProcess(pid);
}

void stepAtBreakPoint(pid_t pid, const Breakpoint &bp) {
  stepProcess(pid);

  auto new_bp = breakProcess(pid, bp.bp_addr);
  assert(new_bp.save_text == bp.save_text);
}

void doCommand(pid_t pid, const std::string &line) {
  static bool running = false;
  static Breakpoint bp;
  auto substrings = splitString(line, ' ');

  if (substrings.size() == 0) return;

  auto cmd = substrings.at(0);
  if (cmd == "b" || cmd == "break") {
    if (substrings.size() != 2) return;
    if (!bp.set) {
      bp = breakProcess(pid, std::stoul(substrings.at(1), nullptr, 0));
    } else {
      std::cout << "already set breakpoint" << std::endl;
    }

  } else if (cmd == "r" || cmd == "run") {
    if (running) {
      std::cout << "The program being debugged has been started already" << std::endl;
    } else {
      continueProcess(pid, bp);
      running = true;
    }

  } else if (cmd == "c" || cmd == "continue") {
    if (!running) {
      std::cout << "The program is not being run" << std::endl;
    } else {
      if (bp.bp_addr == getPC(pid)) {
        stepAtBreakPoint(pid, bp);
      }
      continueProcess(pid, bp);
    }

  } else if (cmd == "s" || cmd == "step") {
    if (!running) {
      std::cout << "The program is not being run" << std::endl;
    } else {
      if (bp.bp_addr == getPC(pid)) {
        stepAtBreakPoint(pid, bp);
      } else {
        stepProcess(pid);
      }
    }

  } else if (cmd == "p" || cmd == "print") {
    if (substrings.size() != 2) return;
    if (substrings.at(1) == std::string("$pc")) {
      std::cout << "0x" << std::hex << getPC(pid) << std::dec << std::endl;
    }

  } else {
    std::cout << "unknown command: " << cmd << std::endl;
  }
}

void mainLoop(pid_t pid) {
  while (true) {
    std::string line;

    std::cout << "(sdb) ";
    std::getline(std::cin, line);

    if (line == std::string("q") || line == std::string("quit")) break;

    doCommand(pid, line);
  }
}

void startDebug(pid_t pid) {
  mainLoop(pid);

  detachProcess(pid);
  (void)kill(pid, SIGTERM);
}

}  // namespace

int main(int argc, char *argv[]) {
  char **child_argv;
  pid_t pid;

  child_argv = (char **)alloca(sizeof(char *) * argc);

  for (int i = 0; i < argc - 1; ++i) {
    child_argv[i] = argv[i + 1];
  }
  child_argv[argc - 1] = nullptr;

  pid = fork();

  if (pid == 0) {
    (void)ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
    execv(child_argv[0], child_argv);
    if (errno == ENOENT)
      execvp(child_argv[0], child_argv);
  }

  (void)waitpid(pid, nullptr, 0);
  startDebug(pid);

  return EXIT_SUCCESS;
}

