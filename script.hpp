#ifndef script_hpp
#define script_hpp

#include "patch.hpp"
#include "hook.hpp"

class Script {
public:
    Memory& memory;
    Patch* patch = 0;
    int scene = 0;
    
    vm_address_t script_addr = 0;
    
    vm_address_t data_offset = 0;
    vm_address_t supporting_func_offset = 0;
    vm_address_t func_delay = 0;
    vm_address_t func_prejudge = 0;
    vm_address_t func_until = 0;
    
    vm_address_t v_delay_start_array;
    vm_address_t v_delay_start_ptr;
    vm_address_t v_operation_done_array;
    vm_address_t v_operation_done_ptr;
    vm_address_t v_refresh_time;
    
    asmjit::Label script_end;
public:
    Script(Memory& memory) : memory(memory) {}
    void init() {
        using namespace asmjit;
        using namespace asmjit::x86;
        // alloc script memory
        vm_address_t addr = memory.Allocate(1024*64, VM_PROT_ALL);
        script_addr = addr;
        // init data, global variable offset
        data_offset = addr + 1024*32;
        v_delay_start_array = data_offset;
        v_delay_start_ptr = v_delay_start_array;
        v_operation_done_array = data_offset + 1024*2;
        v_operation_done_ptr = v_operation_done_array;
        v_refresh_time = data_offset + 1024*4;
        // supporting function
        supporting_func_offset = addr + 1024*48;
        // get scene
        scene = memory.ReadMemory<int>({0x35ee64, 0x780, 0x5540});
        // init internal function
        func_init_delay();
        func_init_prejudge();
        func_init_until();
        // script
        patch = new Patch(memory, addr);
        auto* script = get_assembler();
        script->push(ebp);
        script->mov(ebp, esp);
        script->sub(esp, 0x38);
        // script label
        script_end = script->newLabel();
    }
    asmjit::x86::Assembler* get_assembler() {
        return patch->as;
    }
    void on() {
        using namespace asmjit;
        using namespace asmjit::x86;
        auto* as = get_assembler();
        // end of script
        as->bind(script_end);
        // jmp back
        as->jmp(0x1d364);
        patch->patch();
        // init data, global variable
        // delay state set -1 (unused)
        for (vm_address_t i = v_delay_start_array; i < v_delay_start_ptr; i += 4) {
            memory.WriteMemory(-1, {i});
        }
        // operation done set 0 (not operated)
        for (vm_address_t i = v_operation_done_array; i < v_operation_done_ptr; ++i) {
            memory.WriteMemory<unsigned char>(0, {i});
        }
        // origin func: jmp to script
        Patch origin(memory, 0x1d35e);
        auto& as_origin = origin.get_assembler();
        as_origin.long_().jmp(script_addr);
        origin.patch();
    }
    void off() {
        using namespace asmjit;
        using namespace asmjit::x86;
        unsigned char has_script = memory.ReadMemory<unsigned char>({0x1d35e});
        if (has_script == 0xe9) {
            // free the patch memory
            int addr = memory.ReadMemory<int>({0x1d35f}) + 0x1d35e + 5;
            memory.Free(addr, 1024*64);
            // restore origin code
            Patch origin(memory, 0x1d35e);
            auto& as = origin.get_assembler();
            as.push(ebp);
            as.mov(ebp, esp);
            as.sub(esp, 0x38);
            origin.patch();
        }
    }
private: // do not call these
    void click_scene(int x, int y, int key) {
        using namespace asmjit;
        using namespace asmjit::x86;
        auto* as = get_assembler();
        as->mov(dword_ptr(esp, 0x10), key);
        as->mov(dword_ptr(esp, 0x8), y);
        as->mov(dword_ptr(esp, 0x4), x);
        as->mov(eax, dword_ptr(ebp, 0x8));
        as->mov(dword_ptr(esp), eax);
        as->call(0x2c3fa);
    }
    void click_grid(float row, float col) {
        int x = 80 * col;
        int y;
        if (scene == 2 || scene == 3) {
            y = 55 + 85 * row;
        }else if (scene == 4 || scene == 5) {
            if (col >= 6) {
                y = 45 + 85 * row;
            }else {
                y = 45 + 85 * row + 20 * (6 - col);
            }
        }else {
            y = 40 + 100 * row;
        }
        click_scene(x, y, 1);
    }
    void click_slot(int slot) {
        int x = 63 + 51 * slot;
        int y = 12;
        click_scene(x, y, 1);
    }
    void safe_click() {
        click_scene(0, 0, -1);
    }
public:
#define OPERATION_BEGIN \
using namespace asmjit; \
using namespace asmjit::x86; \
auto* as = get_assembler(); \
Label op_end = as->newLabel(); \
as->mov(eax, byte_ptr(v_operation_done_ptr)); \
as->test(al, al); \
as->jne(op_end);
    
#define OPERATION_END \
as->mov(byte_ptr(v_operation_done_ptr), 0x1); \
as->bind(op_end); \
v_operation_done_ptr += 1;
    void shovel(float row, float col) {
        OPERATION_BEGIN
        safe_click();
        click_scene(640, 36, 1);
        click_grid(row, col);
        OPERATION_END
    }
    void pao(float pao_r, float pao_c, float target_r, float target_c) {
        OPERATION_BEGIN
        safe_click();
        click_grid(pao_r, pao_c);
        click_grid(target_r, target_c);
        OPERATION_END
    }
    void use_card(int slot, float row, float col) {
        OPERATION_BEGIN
        safe_click();
        click_slot(slot);
        click_grid(row, col);
        OPERATION_END
    }
    // bool asm_delay(PvZ*, int gt, int* end_time); returns 1 if time arrives
    void func_init_delay() {
        using namespace asmjit;
        using namespace asmjit::x86;
        // delay func
        func_delay = supporting_func_offset;
        
        Patch delay(memory, func_delay);
        auto& as = delay.get_assembler();
        Label delay_true = as.newLabel();
        Label delay_false = as.newLabel();
        Label delay_cmp = as.newLabel();
        
        as.push(ebp);
        as.mov(ebp, esp);
        as.mov(eax, dword_ptr(ebp, 0x10)); // int* end_time
        as.mov(eax, dword_ptr(eax));
        as.cmp(eax, -1); // unused
        as.jne(delay_cmp);
        
        // get current game time, store in v_delay_start_ptr
        as.mov(eax, dword_ptr(ebp, 0x8)); // arg1: PvZ*
        as.mov(eax, dword_ptr(eax, 0x5560)); // game time
        as.add(eax, dword_ptr(ebp, 0xc)); // arg2: int gt
        as.mov(edx, dword_ptr(ebp, 0x10)); // int* end_time
        as.mov(dword_ptr(edx), eax);
        
        as.bind(delay_cmp);
        as.mov(eax, dword_ptr(ebp, 0x10)); // int* end_time
        as.mov(eax, dword_ptr(eax));
        // get current game time, store in edx
        as.mov(edx, dword_ptr(ebp, 0x8)); // arg1: PvZ*
        as.mov(edx, dword_ptr(edx, 0x5560)); // game time
        as.cmp(eax, edx); // time <= current time ?
        as.jle(delay_true);
        
        as.mov(eax, 0x0);
        as.jmp(delay_false);
        
        as.bind(delay_true);
        as.mov(eax, 0x1);
        
        as.bind(delay_false);
        as.leave();
        as.ret();
        
        delay.patch();
        // next func
        supporting_func_offset += delay.code_length();
    }
    void delay(int gt) {
        using namespace asmjit;
        using namespace asmjit::x86;
        auto* as = get_assembler();
        
        as->mov(dword_ptr(esp, 0x8), (int)v_delay_start_ptr);
        as->mov(dword_ptr(esp, 0x4), gt);
        as->mov(eax, dword_ptr(ebp, 0x8));
        as->mov(dword_ptr(esp), eax);
        as->call((int)func_delay);
        as->cmp(eax, 0x1);
        as->jne(script_end);
        
        v_delay_start_ptr += 4;
    }
    // void asm_prejudge(PvZ*, int gt, int wave, int* end_time);
    void func_init_prejudge() {
        using namespace asmjit;
        using namespace asmjit::x86;
        // prejudge func
        func_prejudge = supporting_func_offset;
        
        Patch prejudge(memory, func_prejudge);
        auto& as = prejudge.get_assembler();
        Label prejudge_eq = as.newLabel();
        Label prejudge_le = as.newLabel();
        Label prejudge_eq_minus_one = as.newLabel();
        Label prejudge_end = as.newLabel();
        Label prejudge_true = as.newLabel();
        Label wave_one = as.newLabel();
        Label wave_cntdown = as.newLabel();
        Label prejudge_wait = as.newLabel();
        Label huge_wave = as.newLabel();
        
        // arg
        Mem pvz_ptr = dword_ptr(ebp, 0x8);
        Mem gt = dword_ptr(ebp, 0xc);
        Mem wave = dword_ptr(ebp, 0x10);
        Mem end_time = dword_ptr(ebp, 0x14);
        
        // var
        Mem curwave = dword_ptr(ebp, -0xc);
        Mem curtime = dword_ptr(ebp, -0x10);
        Mem global_refresh_point = dword_ptr(v_refresh_time);
        
        as.push(ebp);
        as.mov(ebp, esp);
        as.sub(esp, 0x30);
        
        as.mov(edx, pvz_ptr); // arg1: PvZ*
        as.mov(edx, dword_ptr(edx, 0x5570)); // current wave
        as.mov(curwave, edx);
        as.mov(eax, wave); // arg3: int wave
        as.cmp(edx, eax); // curwave < wave
        as.je(prejudge_eq);
        as.jl(prejudge_le);
        as.dec(eax);
        as.cmp(edx, eax);
        as.je(prejudge_eq_minus_one);
        as.jmp(prejudge_true);
        
        as.bind(prejudge_le);
        as.mov(eax, 0x0);
        as.jmp(prejudge_end);
        
        as.bind(prejudge_eq_minus_one);
        // huge wave ?
        as.mov(eax, wave);
        as.cmp(eax, 10);
        as.je(huge_wave);
        as.cmp(eax, 20);
        as.je(huge_wave);
        // normal wave
        // set refresh_time
        as.cmp(eax, 0x1); // wave 1
        as.je(wave_one);
        as.mov(edx, 200);
        as.jmp(wave_cntdown);
        as.bind(wave_one);
        as.mov(edx, 599);
        // count down to refresh
        as.bind(wave_cntdown);
        as.mov(eax, pvz_ptr);
        as.mov(eax, dword_ptr(eax, 0x5590)); // refresh count down
        // eax = cur_refresh, edx = cnt_down
        as.cmp(eax, edx); // cur_refresh > cnt_down
        as.jg(prejudge_le);
        as.jmp(prejudge_wait);
        
        // huge wave
        as.bind(huge_wave);
        as.mov(edx, pvz_ptr);
        as.mov(edx, dword_ptr(edx, 0x5590)); // refresh count down
        as.cmp(edx, 5); // edx = refresh count down
        as.jg(prejudge_le);
        as.mov(eax, pvz_ptr);
        as.mov(eax, dword_ptr(eax, 0x5598)); // huge wave count down
        as.cmp(eax, 750);
        as.jg(prejudge_le);
        as.cmp(edx, 4);
        as.je(prejudge_wait);
        as.cmp(edx, 5);
        as.je(prejudge_wait);
        as.add(eax, 745); // now eax eq to cur_refresh
        
        // eax = cur_refresh
        as.bind(prejudge_wait);
        as.mov(edx, pvz_ptr);
        as.mov(edx, dword_ptr(edx, 0x5560)); // game time
        // eax = cur_refresh, edx = clock
        as.add(edx, eax);
        as.mov(global_refresh_point, edx); // global_refresh_point = clock + cur_refresh
        as.mov(edx, gt);
        // eax = cur_refresh, edx = gt
        as.add(edx, eax);
        // edx = wait_time
        as.mov(dword_ptr(esp, 0x4), edx);
        as.mov(eax, end_time);
        as.mov(dword_ptr(esp, 0x8), eax);
        as.mov(eax, pvz_ptr);
        as.mov(dword_ptr(esp), eax);
        as.call((int)func_delay);
        as.jmp(prejudge_end);
        
        as.bind(prejudge_eq);
        as.mov(eax, pvz_ptr);
        as.mov(eax, dword_ptr(eax, 0x5560)); // game time
        as.mov(curtime, eax);
        as.mov(eax, pvz_ptr);
        as.mov(eax, dword_ptr(eax, 0x5590)); // refresh count down
        as.mov(edx, pvz_ptr);
        as.mov(edx, dword_ptr(edx, 0x5594)); // wave interval (init_cntdown)
        as.sub(edx, eax); // edx = init_cntdown - cur_refresh
        as.mov(eax, curtime);
        as.sub(eax, edx);
        as.mov(global_refresh_point, eax);
        as.mov(eax, gt);
        as.sub(eax, edx);
        as.mov(dword_ptr(esp, 0x4), eax); // wait_time
        as.mov(eax, end_time);
        as.mov(dword_ptr(esp, 0x8), eax);
        as.mov(eax, pvz_ptr);
        as.mov(dword_ptr(esp), eax);
        as.call((int)func_delay);
        as.jmp(prejudge_end);
        
        as.bind(prejudge_true);
        as.mov(eax, 0x1);
        
        as.bind(prejudge_end);
        as.add(esp, 0x30);
        as.leave();
        as.ret();
        
        prejudge.patch();
        // next func
        supporting_func_offset += prejudge.code_length();
    }
    void prejudge(int gt, int wave) {
        using namespace asmjit;
        using namespace asmjit::x86;
        auto* as = get_assembler();
        
        as->mov(dword_ptr(esp, 0xc), (int)v_delay_start_ptr);
        as->mov(dword_ptr(esp, 0x8), wave);
        as->mov(dword_ptr(esp, 0x4), gt);
        as->mov(eax, dword_ptr(ebp, 0x8));
        as->mov(dword_ptr(esp), eax);
        as->call((int)func_prejudge);
        as->cmp(eax, 0x1);
        as->jne(script_end);
        
        v_delay_start_ptr += 4;
    }
    // bool asm_until(PvZ*, int gt, int* end_time); returns 1 if time arrives
    void func_init_until() {
        using namespace asmjit;
        using namespace asmjit::x86;
        // until func
        func_until = supporting_func_offset;
        
        Patch until(memory, func_until);
        auto& as = until.get_assembler();
        
        // arg
        Mem pvz_ptr = dword_ptr(ebp, 0x8);
        Mem gt = dword_ptr(ebp, 0xc);
        Mem end_time = dword_ptr(ebp, 0x10);
        
        // var
        Mem global_refresh_point = dword_ptr(v_refresh_time);
        
        as.push(ebp);
        as.mov(ebp, esp);
        as.mov(edx, pvz_ptr);
        as.mov(edx, dword_ptr(edx, 0x5560)); // game time
        as.sub(edx, global_refresh_point);
        as.mov(eax, gt);
        as.sub(eax, edx);
        as.mov(dword_ptr(esp, 0x4), eax);
        as.mov(eax, end_time);
        as.mov(dword_ptr(esp, 0x8), eax);
        as.mov(eax, pvz_ptr);
        as.mov(dword_ptr(esp), eax);
        as.call((int)func_delay);
        as.leave();
        as.ret();
        
        until.patch();
        // next func
        supporting_func_offset += until.code_length();
    }
    void until(int gt) {
        using namespace asmjit;
        using namespace asmjit::x86;
        auto* as = get_assembler();
        
        as->mov(dword_ptr(esp, 0x8), (int)v_delay_start_ptr);
        as->mov(dword_ptr(esp, 0x4), gt);
        as->mov(eax, dword_ptr(ebp, 0x8));
        as->mov(dword_ptr(esp), eax);
        as->call((int)func_until);
        as->cmp(eax, 0x1);
        as->jne(script_end);
        
        v_delay_start_ptr += 4;
    }
};

class BugFix {
    Memory& memory;
public:
    BugFix(Memory& memory) : memory(memory) {}
    void Fix() {
        FixUseCard();
        FixClickCard();
        FixMushroomWeakUp();
    }
    void FixUseCard() {
        using namespace asmjit;
        using namespace asmjit::x86;
        uint32_t patch_addr = (uint32_t)memory.Allocate(1024, VM_PROT_ALL);
        /*
         2a260: (length=6)
         jmp patch
         */
        {
            Patch patch(memory, 0x2a260);
            auto& as = *patch.as;
            
            as.jmp(patch_addr);
            
            assert(patch.code_length() <= 6);
            patch.patch();
        }
        /*
         patch:
         cmp dword [ebp+0x14], 0x0
         js 2a266       // 右键取消卡片选择
         mov eax, dword [ebp+0x8]
         mov dword [esp], eax
         call get_hold_card_id_117ec
         cmp eax, 0x34  // card_id <= 0x34: plant
         jle 2a2cf      // 放植物
         cmp eax, 0x3c
         jl 2a266       // 取消放置
         cmp eax, 0x4a
         jg 2a266       // 取消放置
         jmp 2a2a4      // 放僵尸
         */
        {
            Patch patch(memory, patch_addr);
            auto& as = *patch.as;
            
            as.cmp(dword_ptr(ebp, 0x14), 0x0);
            as.js(0x2a266);
            
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(dword_ptr(esp), eax);
            as.call(0x117ec);
            as.cmp(eax, 0x34);
            as.jle(0x2a2cf);
            as.cmp(eax, 0x3c);
            as.jl(0x2a266);
            as.cmp(eax, 0x4a);
            as.jg(0x2a266);
            as.jmp(0x2a2a4);
            
            patch.patch();
        }
    }
    
    void FixClickCard() {
        using namespace asmjit;
        using namespace asmjit::x86;
        uint32_t patch_addr = (uint32_t)memory.Allocate(1024, VM_PROT_ALL);
        /*
         0xf8a9e: (length=6)
         jmp patch
         */
        {
            Patch patch(memory, 0xf8a9e);
            auto& as = patch.get_assembler();
            
            as.jmp(patch_addr);
            
            assert(patch.code_length() <= 6);
            patch.patch();
        }
        /*
         patch:
         jle f96c6 // id not exist
         
         cmp eax, 0x38
         je sun
         cmp eax, 0x39
         je diamond
         cmp eax, 0x3a
         je zombie
         cmp eax, 0x3b
         je cup
         
         cmp eax, 0x4a
         jg f96c6 // id not exist
         cmp eax, 0x34
         jg not_plant
         jmp f8aa4 // plant
         not_plant:
         cmp eax, 0x3b
         jg f8aa4 // zombie
         
         mov eax, dword [ebp+0x8]
         mov eax, dword [eax]
         mov eax, dword [eax+0x7c0]
         cmp eax, 0x14 // 宝石迷阵
         je f8aa4
         cmp eax, 0x18 // 宝石迷阵2
         je f8aa4
         jne f96c6 // id may crash game
         
         cup:
         mov eax, dword [ebp+0x8]
         mov eax, dword [eax+0x4]
         mov eax, dword [eax+0x154]
         mov dword [esp], eax
         mov dword [esp+0x4], 0x2
         mov dword [esp+0x8], 0x0
         call a6164
         jmp f96c6
         sun:
         mov eax, dword [ebp+0x8]
         mov eax, dword [eax+0x4]
         mov edx, dword [eax+0x5554]
         lea edx, dword [edx+1000]
         mov dword [eax+0x5554], edx
         jmp f96c6
         diamond:
         mov eax, dword [ebp+0x8]
         mov eax, dword [eax+0x4]
         mov dword [esp], eax
         mov dword [esp+0x4], 400
         mov dword [esp+0x8], 300
         mov dword [esp+0xc], 3
         mov dword [esp+0x10], 3
         call 28e02
         jmp f96c6
         zombie:
         mov eax, dword [ebp+0x8]
         mov eax, dword [eax]
         mov eax, dword [eax+0x7c0]
         cmp eax, 0x17
         je f8aa4
         mov dword [esp+0xc], 0
         mov dword [esp+0x8], -2
         mov dword [esp+0x4], 0xb
         mov eax, dword [ebp+0x8]
         mov eax, dword [eax+0x4]
         mov eax, dword [eax+0x154]
         mov dword [esp], eax
         call ac672
         jmp f96c6
         */
        {
            Patch patch(memory, patch_addr);
            auto& as = patch.get_assembler();
            Label cup = as.newLabel();
            Label not_plant = as.newLabel();
            Label sun = as.newLabel();
            Label diamond = as.newLabel();
            Label zombie = as.newLabel();
            uint32_t discard = 0xf96c6;
            uint32_t go_on = 0xf8aa4;
            
            as.jle(discard);
            as.cmp(eax, 0x38);
            as.je(sun);
            as.cmp(eax, 0x39);
            as.je(diamond);
            as.cmp(eax, 0x3a);
            as.je(zombie);
            as.cmp(eax, 0x3b);
            as.je(cup);
            
            as.cmp(eax, 0x4a);
            as.jg(discard);
            as.cmp(eax, 0x34);
            as.jg(not_plant);
            as.jmp(go_on);
            
            as.bind(not_plant);
            as.cmp(eax, 0x3b);
            as.jg(go_on);
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(eax, dword_ptr(eax));
            as.mov(eax, dword_ptr(eax, 0x7c0));
            as.cmp(eax, 0x14);
            as.je(go_on);
            as.cmp(eax, 0x18);
            as.je(go_on);
            as.jmp(discard);
            
            as.bind(cup);
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(eax, dword_ptr(eax, 0x4));
            as.mov(eax, dword_ptr(eax, 0x154));
            as.mov(dword_ptr(esp), eax);
            as.mov(dword_ptr(esp, 0x4), 0x2);
            as.mov(dword_ptr(esp, 0x8), 0x0);
            as.call(0xa6164);
            as.jmp(discard);
            
            as.bind(sun);
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(eax, dword_ptr(eax, 0x4));
            as.mov(edx, dword_ptr(eax, 0x5554));
            as.lea(edx, dword_ptr(edx, 1000));
            as.mov(dword_ptr(eax, 0x5554), edx);
            as.jmp(discard);
            
            as.bind(diamond);
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(eax, dword_ptr(eax, 0x4));
            as.mov(dword_ptr(esp), eax);
            as.mov(dword_ptr(esp, 0x4), 400);
            as.mov(dword_ptr(esp, 0x8), 300);
            as.mov(dword_ptr(esp, 0xc), 3);
            as.mov(dword_ptr(esp, 0x10), 3);
            as.call(0x28e02);
            as.jmp(discard);
            
            as.bind(zombie);
            /*
             mov eax, dword [ebp+0x8]
             mov eax, dword [eax]
             mov eax, dword [eax+0x7c0]
             cmp eax, 0x17
             je f8aa4
             */
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(eax, dword_ptr(eax));
            as.mov(eax, dword_ptr(eax, 0x7c0));
            as.cmp(eax, 0x17);
            as.je(go_on);
            as.mov(dword_ptr(esp, 0xc), 0);
            as.mov(dword_ptr(esp, 0x8), -2);
            as.mov(dword_ptr(esp, 0x4), 0xb);
            as.mov(eax, dword_ptr(ebp, 0x8));
            as.mov(eax, dword_ptr(eax, 0x4));
            as.mov(eax, dword_ptr(eax, 0x154));
            as.mov(dword_ptr(esp), eax);
            as.call(0xac672);
            as.jmp(discard);
            
            patch.patch();
        }
    }
    void FixMushroomWeakUp() {
        /*
         3d4b7:
         jmp patch
         
         patch:
         // eax = mushroom
         mov edx, dword [ebp+0x8] // coffee
         cmp eax, edx
         jl rectify // mushroom < coffee
         mov dword [eax+0x130], 0x64
         jmp 3d4c1
         
         rectify:
         mov dword [eax+0x130], 0x63
         jmp 3d4c1
         */
        using namespace asmjit;
        using namespace asmjit::x86;
        uint32_t patch_addr = (uint32_t)memory.Allocate(1024, VM_PROT_ALL);
        {
            Patch patch(memory, 0x3d4b7);
            auto& as = patch.get_assembler();
            as.jmp(patch_addr);
            patch.patch();
        }
        {
            Patch patch(memory, patch_addr);
            auto& as = patch.get_assembler();
            Label rectify = as.newLabel();
            
            as.mov(edx, dword_ptr(ebp, 0x8));
            as.cmp(eax, edx);
            as.jl(rectify);
            as.mov(dword_ptr(eax, 0x130), 0x64);
            as.jmp(0x3d4c1);
            
            as.bind(rectify);
            as.mov(dword_ptr(eax, 0x130), 0x63);
            as.jmp(0x3d4c1);
            
            patch.patch();
        }
        
    }
};

#endif /* script_hpp */
