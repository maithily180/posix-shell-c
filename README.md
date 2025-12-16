## 1. C Shell Implementation

### Overview
A fully-featured shell implementation supporting:
- Command parsing with Context-Free Grammar
- Built-in commands (hop, reveal, log)
- File redirection and piping
- Background and sequential execution
- Job control and signal handling

### Features Implemented

#### Part A: Shell Input (65 marks)
- **A.1 Shell Prompt**: Dynamic prompt showing `<Username@SystemName:current_path>`
- **A.2 User Input**: Interactive command input handling
- **A.3 Input Parsing**: CFG-based command validation

#### Part B: Shell Intrinsics (70 marks)
- **B.1 hop**: Directory navigation with `~`, `.`, `..`, `-` support
- **B.2 reveal**: File listing with `-a` and `-l` flags
- **B.3 log**: Command history with persistent storage

#### Part C: File Redirection and Pipes (200 marks)
- **C.1 Command Execution**: External command execution
- **C.2 Input Redirection**: `<` operator for file input
- **C.3 Output Redirection**: `>` and `>>` operators
- **C.4 Command Piping**: `|` operator for command chaining

#### Part D: Sequential and Background Execution (200 marks)
- **D.1 Sequential Execution**: `;` operator for command sequences
- **D.2 Background Execution**: `&` operator for background processes

#### Part E: Exotic Shell Intrinsics (110 marks)
- **E.1 activities**: List running/stopped processes
- **E.2 ping**: Send signals to processes
- **E.3 Signal Handling**: Ctrl-C, Ctrl-D, Ctrl-Z support
- **E.4 Job Control**: `fg` and `bg` commands

### Compilation and Usage

```bash
# Navigate to shell directory
cd shell/

# Compile the shell
make all

# Run the shell
./shell.out
```

### Compilation Flags
The shell is compiled with strict POSIX compliance:
```bash
gcc -std=c99 \
  -D_POSIX_C_SOURCE=200809L \
  -D_XOPEN_SOURCE=700 \
  -Wall -Wextra -Werror \
  -Wno-unused-parameter \
  -fno-asm
```

### Example Usage
```bash
<user@system:~> echo "Hello World" > test.txt
<user@system:~> cat test.txt | grep Hello
Hello World
<user@system:~> sleep 5 &
[1] 12345
<user@system:~> activities
[12345] : sleep - Running
```

---