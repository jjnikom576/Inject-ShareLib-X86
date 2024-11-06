#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/mman.h>
#include <android/log.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/user.h>

#define LOG_TAG "Injector"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class ScopedPtrace {
public:
    ScopedPtrace(pid_t pid) : pid(pid) {
        attached = (ptrace(PTRACE_ATTACH, pid, NULL, NULL) != -1);
        if (attached) {
            LOGD("Successfully attached to process %d", pid);
            waitpid(pid, NULL, 0);
        } else {
            LOGE("Failed to attach to process %d: %s", pid, strerror(errno));
        }
    }
    
    ~ScopedPtrace() {
        if (attached) {
            ptrace(PTRACE_DETACH, pid, NULL, NULL);
            LOGD("Detached from process %d", pid);
        }
    }
    
    bool isAttached() { return attached; }
    
    bool getRegs(struct user_regs_struct* regs) {
        return ptrace(PTRACE_GETREGS, pid, NULL, regs) != -1;
    }
    
    bool setRegs(struct user_regs_struct* regs) {
        return ptrace(PTRACE_SETREGS, pid, NULL, regs) != -1;
    }
    
    bool cont() {
        return ptrace(PTRACE_CONT, pid, NULL, NULL) != -1;
    }

private:
    pid_t pid;
    bool attached;
};

class ProcessMemory {
public:
    ProcessMemory(pid_t pid) : pid(pid), memfd(-1) {
        char path[32];
        snprintf(path, sizeof(path), "/proc/%d/mem", pid);
        memfd = open(path, O_RDWR);
        if (memfd == -1) {
            LOGE("Failed to open process memory: %s", strerror(errno));
        }
    }
    
    ~ProcessMemory() {
        if (memfd != -1) close(memfd);
    }
    
    bool isValid() { return memfd != -1; }
    
    bool WriteMemory(const void* source, uintptr_t destination, size_t size) {
        if (lseek64(memfd, destination, SEEK_SET) < 0) {
            LOGE("Failed to seek to address %p: %s", (void*)destination, strerror(errno));
            return false;
        }
        ssize_t written = write(memfd, source, size);
        if (written != size) {
            LOGE("Failed to write memory at %p: %s", (void*)destination, strerror(errno));
            return false;
        }
        return true;
    }
    
    bool ReadMemory(uintptr_t source, void* destination, size_t size) {
        if (lseek64(memfd, source, SEEK_SET) < 0) {
            LOGE("Failed to seek to address %p: %s", (void*)source, strerror(errno));
            return false;
        }
        ssize_t bytes_read = read(memfd, destination, size);
        if (bytes_read != size) {
            LOGE("Failed to read memory at %p: %s", (void*)source, strerror(errno));
            return false;
        }
        return true;
    }
    
    uintptr_t FindExecutableMemory(size_t size) {
        char maps_path[32];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
        
        FILE* fp = fopen(maps_path, "r");
        if (!fp) {
            LOGE("Failed to open maps file: %s", strerror(errno));
            return 0;
        }
        
        char line[512];
        uintptr_t addr = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "r-xp") || strstr(line, "rwxp")) {
                uintptr_t start, end;
                sscanf(line, "%lx-%lx", &start, &end);
                if (end - start >= size) {
                    addr = start;
                    LOGD("Found suitable memory region: %s", line);
                    break;
                }
            }
        }
        fclose(fp);
        return addr;
    }

private:
    pid_t pid;
    int memfd;
};

bool InjectLibrary(pid_t target_pid, const char* library_path) {
    LOGD("Starting injection into process %d with library %s", target_pid, library_path);
    
    ProcessMemory proc(target_pid);
    if (!proc.isValid()) {
        LOGE("Failed to open process memory");
        return false;
    }

    // Get dlopen address
    void* handle = dlopen("libdl.so", RTLD_LAZY);
    if (!handle) {
        LOGE("Failed to open libdl.so: %s", dlerror());
        return false;
    }
    
    void* dlopen_addr = dlsym(handle, "dlopen");
    if (!dlopen_addr) {
        LOGE("Failed to find dlopen: %s", dlerror());
        dlclose(handle);
        return false;
    }
    
    LOGD("Found dlopen at address %p", dlopen_addr);
    
    // Calculate shellcode size
    size_t path_len = strlen(library_path) + 1;
    size_t shellcode_size = 64 + path_len;  // Increased size for safety
    
    // Allocate memory for shellcode
    uint8_t* shellcode = new uint8_t[shellcode_size];
    memset(shellcode, 0, shellcode_size);
    
    // Copy library path to the end of shellcode
    strcpy((char*)shellcode + 64, library_path);
    
    // Create shellcode
    #ifdef __x86_64__
    // x64 shellcode
    // mov rdi, path_address
    shellcode[0] = 0x48;  // REX.W
    shellcode[1] = 0xBF;  // mov rdi
    *(uintptr_t*)(shellcode + 2) = (uintptr_t)shellcode + 64;  // path address will be fixed later
    
    // mov rsi, RTLD_NOW | RTLD_GLOBAL
    shellcode[10] = 0x48;  // REX.W
    shellcode[11] = 0xBE;  // mov rsi
    *(uintptr_t*)(shellcode + 12) = RTLD_NOW | RTLD_GLOBAL;
    
    // call dlopen
    shellcode[20] = 0xE8;
    // address will be fixed later
    
    // ret
    shellcode[25] = 0xC3;
    #else
    // x86 shellcode
    // push RTLD_NOW | RTLD_GLOBAL
    shellcode[0] = 0x68;
    *(uint32_t*)(shellcode + 1) = RTLD_NOW | RTLD_GLOBAL;
    
    // push path_address
    shellcode[5] = 0x68;
    // address will be fixed later
    
    // call dlopen
    shellcode[10] = 0xE8;
    // address will be fixed later
    
    // add esp, 8
    shellcode[15] = 0x83;
    shellcode[16] = 0xC4;
    shellcode[17] = 0x08;
    
    // ret
    shellcode[18] = 0xC3;
    #endif
    
    // Find executable memory
    uintptr_t exec_addr = proc.FindExecutableMemory(shellcode_size);
    if (!exec_addr) {
        LOGE("Failed to find executable memory");
        delete[] shellcode;
        return false;
    }
    
    LOGD("Found executable memory at %p", (void*)exec_addr);
    
    // Fix addresses in shellcode
    #ifdef __x86_64__
    *(uintptr_t*)(shellcode + 2) = exec_addr + 64;  // path address
    *(uint32_t*)(shellcode + 21) = (uintptr_t)dlopen_addr - (exec_addr + 25);  // relative call address
    #else
    *(uintptr_t*)(shellcode + 6) = exec_addr + 64;  // path address
    *(uint32_t*)(shellcode + 11) = (uintptr_t)dlopen_addr - (exec_addr + 15);  // relative call address
    #endif
    
    // Backup original memory
    uint8_t* backup = new uint8_t[shellcode_size];
    if (!proc.ReadMemory(exec_addr, backup, shellcode_size)) {
        LOGE("Failed to backup memory");
        delete[] shellcode;
        delete[] backup;
        return false;
    }
    
    // Write shellcode
    if (!proc.WriteMemory(shellcode, exec_addr, shellcode_size)) {
        LOGE("Failed to write shellcode");
        delete[] shellcode;
        delete[] backup;
        return false;
    }
    
    delete[] shellcode;
    
    // Execute shellcode
    struct user_regs_struct regs, original_regs;
    ScopedPtrace ptrace(target_pid);
    
    if (!ptrace.isAttached()) {
        LOGE("Failed to attach to process");
        delete[] backup;
        return false;
    }
    
    if (!ptrace.getRegs(&original_regs)) {
        LOGE("Failed to get registers");
        delete[] backup;
        return false;
    }
    
    memcpy(&regs, &original_regs, sizeof(regs));
    
    #ifdef __x86_64__
    regs.rip = exec_addr;
    #else
    regs.eip = exec_addr;
    #endif
    
    if (!ptrace.setRegs(&regs)) {
        LOGE("Failed to set registers");
        delete[] backup;
        return false;
    }
    
    LOGD("Executing shellcode");
    
    if (!ptrace.cont()) {
        LOGE("Failed to continue process");
        delete[] backup;
        return false;
    }
    
    waitpid(target_pid, NULL, 0);
    
    // Check result
    if (!ptrace.getRegs(&regs)) {
        LOGE("Failed to get registers after execution");
        delete[] backup;
        return false;
    }
    
    #ifdef __x86_64__
    uintptr_t result = regs.rax;
    #else
    uintptr_t result = regs.eax;
    #endif
    
    if (result == 0) {
        LOGE("dlopen returned NULL - library load failed");
        return false;
    }
    
    LOGD("dlopen returned: %p", (void*)result);
    
    // Restore registers
    if (!ptrace.setRegs(&original_regs)) {
        LOGE("Failed to restore registers");
        delete[] backup;
        return false;
    }
    
    // Restore memory
    if (!proc.WriteMemory(backup, exec_addr, shellcode_size)) {
        LOGE("Failed to restore memory");
        delete[] backup;
        return false;
    }
    
    delete[] backup;
    
    LOGD("Successfully injected library");
    return true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <pid> <library path>\n", argv[0]);
        return 1;
    }
    
    pid_t pid = atoi(argv[1]);
    const char* library_path = argv[2];
    
    if (InjectLibrary(pid, library_path)) {
        printf("Successfully injected %s into process %d\n", library_path, pid);
        return 0;
    } else {
        printf("Failed to inject library\n");
        return 1;
    }
}