#include <iostream>
#include "script.hpp"
Memory memory;
int main() {
    // attach process
    pid_t pid = memory.PidFromName("Plants vs. Zombi");
    memory.Attach(pid);
    
    // fix bug
    BugFix bugfix(memory);
    // 使用ICE3时最好加上，避免因咖啡豆、寒冰菇编号相对大小不同造成寒冰菇唤醒时间的1厘秒的差异
    // 这个函数通过注入代码修正了寒冰菇唤醒时间。
    bugfix.FixMushroomWeakUp();
    
    bool on = true;
    Script script(memory);
    // remove previous script if any
    script.off();
    if (on) {
        // alloc, prepare for code injection, write internal function
        script.init();
        // begin script
        for (int i = 1; i <= 20; ++i) {
            script.prejudge(200, i);
            script.pao(1, 1, 2, 9);
            script.pao(2, 1, 5, 9);
            script.delay(373+106-298);
            script.use_card(1, 1, 3);
            script.use_card(2, 1, 3);
            if (i == 9 || i == 19 || i == 20) {
                script.delay(550);
                script.pao(1, 1, 2, 9);
                script.pao(2, 1, 5, 9);
            }
        }
        printf("code length: %zu\n", script.patch->code_length());
        // write script
        script.on();
    }
    
}
/*
 pe-12:
 for (int i = 1; i <= 20; ++i) {
 script.prejudge(-90, i);
 script.pao(1, 1, 2, 9);
 script.pao(2, 1, 5, 9);
 }
 */
/*
 ice3 for day, pool, roof:
 for (int i = 1; i <= 9; ++i) {
 script.prejudge(200, i);
 script.pao(1, 1, 2, 9);
 script.pao(2, 1, 5, 9);
 // (plant index) ice < coffee, ice activated next gt
 script.delay(373+105-298);
 script.use_card(1, 1, 3);
 script.use_card(2, 1, 3);
 }
 */
/*
 ice3 for night, fog, moon:
 for (int i = 1; i <= 20; ++i) {
 script.prejudge(200, i);
 script.pao(1, 1, 2, 9);
 script.pao(2, 1, 5, 9);
 script.delay(373+106-100);
 script.use_card(1, 1, 3);
 }
 */
