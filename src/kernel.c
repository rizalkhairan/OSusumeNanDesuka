#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/interrupt/interrupt.h"
#include "header/interrupt/idt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"
#include "header/terminal/terminal.h"
#include "header/filesystem/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#include "header/memory/paging.h"
#include "header/process/process.h"
#include "header/process/scheduler.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    // scheduler_init();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_ext2();
    gdt_install_tss();
    set_tss_register();

    // Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);

    // Write shell into memory
    char tes[BLOCK_SIZE];
    struct EXT2DriverRequest request = {
        .buf                   = (uint8_t*) 0,
        .name                  = "shell",
        .parent_inode          = 2,
        .buffer_size           = 0x100000,
        .name_len              = 5,
        .is_directory = 0
    };
    read(request);


    struct EXT2DriverRequest req = {
        .name = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .name_len = 64,
        .parent_inode = 2,
        .buffer_size = 8,
        .is_directory = true,
        .buf = {0},
    };    
    uint32_t x = write(&req);

    struct EXT2DriverRequest req2 = {
        .name = "yoisaki",
        .name_len = 7,
        .parent_inode = 3,
        .buffer_size = 8,
        .is_directory = true,
        .buf = {0},
    };
    uint32_t y = write(&req2);

    struct EXT2DriverRequest reqbro = {
        .name = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        .name_len = 64,
        .parent_inode = 5,
        .buffer_size = 8,
        .is_directory = true,
        .buf = {0},
    };
    uint32_t bro = write(&reqbro);

    struct EXT2DriverRequest req3 = {
        .name = "shinonome",
        .name_len = 9,
        .parent_inode = 2,
        .buffer_size = 8,
        .is_directory = true,
        .buf = {0},
    };
    uint32_t z = write(&req3);

    struct EXT2DriverRequest reqnew = {
        .name = "yoisaki",
        .name_len = 7,
        .parent_inode = 2,
        .buffer_size = 8,
        .is_directory = true,
        .buf = {0},
    };
    uint32_t w = write(&reqnew);

  char nijiiro[430] = "mawaru machikado yukikau hitobito hitori ni hitotsu kakegae no nai story kitto watashi mo sonna fuu ni naritakatta ougon no tsuki yorisou hoshi mo asatsuyu ni nureta hana mo todokanai to okubyou na kokoro ni kagi wo kaketa utsumuku namida ochite hajiketa chiisana niji utsushita kuraun hiroiageru te hitori ja nakatta tonari de utau koe kowagari na senaka wo oshite tojikometa hazu no kokoro ni tsubasa yadosu mahou wo kureta kara";
    struct BlockBuffer file = {0};
    memcpy(file.buf, nijiiro, 430);
    struct EXT2DriverRequest req4 = {
        .name = "nijiiro",
        .name_len = 7,
        .parent_inode = 4,
        .buffer_size = 430,
        .is_directory = false,
        .buf = file.buf,
    };
    uint32_t a = write(&req4);

    char samsa[482] = "tsukaifurushita jibun no namae ni aete kicchu na rubi wo futte kouketsu wo uchi makaseru kurai ni osoroshiku naru hone no zui made ima wa donna fuu ni mietemasu ka? minikui desu ka? sore wa sokka douka ringo wo nagetsukenaide mune ni Lock up Lock up zamuza kagami wo goran dareka ga sasayaku umaku ittara moukemono sa amai kotoba mo egao mo tsuujinai hashiridashitara mou kemono da tsuki no mashita wo urotsuki nagara kangaeteta yosugara akumu ni dono yubi tatete yarubeki ka tte ne";
    struct BlockBuffer file2 = {0};
    memcpy(file2.buf, samsa, 482);
    struct EXT2DriverRequest req5 = {
        .name = "samsa",
        .name_len = 5,
        .parent_inode = 5,
        .buffer_size = 482,
        .is_directory = false,
        .buf = file2.buf,
    };
    uint32_t b = write(&req5);

    char watashiwaame[507] = "watashi wa dare anata no aware yozora no naka de namae wo nakushite uneri no nai minamo ni hisomu keshiki wo shiranai mama (kiri ni natte shimatte mo) tadayou kumo (betsuni ii no ni) kinou made wa (kamawanai no ni) tadayou kumo watashi wa naze massugu ni ochirudareka no tenohira wo sagasu tame sora wo dekiru kagiri me ni osamenagara watashi wa ame (ame ame ame) hajikarete wakaru dareka (dare dare) no you ni wa narenai ame (ame ame ame) chikyuu wo komaraseru hodo no itami wo shiranai kara watashi wa ame";
    struct BlockBuffer file3 = {0};
    memcpy(file3.buf, watashiwaame, 507);
    struct EXT2DriverRequest req6 = {
        .name = "watashiwaame",
        .name_len = 12,
        .parent_inode = 6,
        .buffer_size = 507,
        .is_directory = false,
        .buf = file3.buf,
    };
    uint32_t c = write(&req6);

    char abcd[20] = "ini ceritanya file 1";
    struct BlockBuffer file4 = {0};
    memcpy(file4.buf, abcd, 20);
    struct EXT2DriverRequest req7 = {
        .name = "file1",
        .name_len = 5,
        .parent_inode = 2,
        .buffer_size = 20,
        .is_directory = false,
        .buf = file4.buf,
    };
    uint32_t d = write(&req7);

    set_tss_kernel_current_stack();
    process_create_user_process(request);
    // kernel_execute_user_program((uint8_t*) 0);
    // process_init();
    scheduler_init();
    scheduler_switch_to_next_process();
}