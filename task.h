/*
 * core/task.h
 *
 * Copyright (c) 2019 bzt (bztsrc@gitlab)
 * https://creativecommons.org/licenses/by-nc-sa/4.0/
 *
 * A művet szabadon:
 *
 * - Megoszthatod — másolhatod és terjesztheted a művet bármilyen módon
 *     vagy formában
 * - Átdolgozhatod — származékos műveket hozhatsz létre, átalakíthatod
 *     és új művekbe építheted be. A jogtulajdonos nem vonhatja vissza
 *     ezen engedélyeket míg betartod a licensz feltételeit.
 *
 * Az alábbi feltételekkel:
 *
 * - Nevezd meg! — A szerzőt megfelelően fel kell tüntetned, hivatkozást
 *     kell létrehoznod a licenszre és jelezned, ha a művön változtatást
 *     hajtottál végre. Ezt bármilyen ésszerű módon megteheted, kivéve
 *     oly módon ami azt sugallná, hogy a jogosult támogat téged vagy a
 *     felhasználásod körülményeit.
 * - Ne add el! — Nem használhatod a művet üzleti célokra.
 * - Így add tovább! — Ha feldolgozod, átalakítod vagy gyűjteményes művet
 *     hozol létre a műből, akkor a létrejött művet ugyanazon licensz-
 *     feltételek mellett kell terjesztened, mint az eredetit.
 *
 * @subsystem taszk
 * @brief taszk funkciók
 */

/* prioritási szintek, a hd_active és cr_active sorok indexei */
#define PRI_SYS             0   /* prioritás 0, rendszer, nem megszakítható */
#define PRI_RT              1   /* prioritás 1, valós idejű taszkok */
#define PRI_DRV             2   /* prioritás 2, eszközmeghajtók */
#define PRI_SRV             3   /* prioritás 3, rendszer szolgáltatások */
#define PRI_APPH            4   /* prioritás 4, alkalmazás, magas prioritás */
#define PRI_APP             5   /* prioritás 5, alkalmazás, normál prioritás */
#define PRI_APPL            6   /* prioritás 6, alkalmazás, alacsony prioritás */
#define PRI_IDLE            7   /* prioritás 7, idle sor (defregmentáló, képernyővédő stb.) */

/* taszk állapotok */
#define TCB_STATE_HYBERND   0   /* hibernált, ki van lapolva háttértárra */
#define TCB_STATE_BLOCKED   1   /* blokkolt, várakozik */
#define TCB_STATE_RUNNING   2   /* futásra kész */

/* struktúra mezőinek poziciói, asm-hez */
#define TCBFLD_GPR          (1024)

#ifndef _AS

/*** Taszk Kontrol Blokk. Architectúra független struktúra ***/
typedef struct {
    /*** általános ***/
    uint32_t magic;             /* Taszk Kontrol Blokk azonosító, 'TASK' */
    uint8_t priority;           /* taszk prioritás, lásd PRI_*       tcb+   4 */
    uint8_t state;              /* taszk állapot                     tcb+   5 */
    uint8_t flags;              /* taszk jelzők                      tcb+   6 */
    uint8_t forklevel;          /* forkolási szint                   tcb+   7 */
    uint32_t cpuid;             /* melyik CPU magon fut              tcb+   8 */
    uint32_t cmdline;           /* argv-re mutat a veremben          tcb+  12 */
    /* memória kezelés */
    phy_t memroot;              /* memórialeképezés gyökere          tcb+  16 */
    phy_t taskmemroot;          /* taszkmemórialeképezés gyökere     tcb+  24 */
    uint64_t linkmem;           /* hivatkozott lapok száma           tcb+  32 */
    uint64_t allocmem;          /* lefoglalt lapok száma             tcb+  40 */
    void *elfbin;               /* címtérbe betöltött ELF-ek listája tcb+  48 */
    /* taszk kezelés, ütemezés */
    pid_t pid;                  /* taszk pid                         tcb+  56 */
    pid_t parent;               /* szülő pid                         tcb+  64 */
    pid_t prev;                 /* előző pid ebben a priosorban      tcb+  72 */
    pid_t next;                 /* következő pid ebben a priosorban  tcb+  80 */
    pid_t blksend;              /* küldéskor blokkolódott taszkok    tcb+  88 */
    uint64_t blktime;           /* amikortól blokkolódott            tcb+  96 */
    uint64_t blkcnt;            /* összesen mennyit blokkolódott     tcb+ 104 */
    uint64_t alarmusec;         /* mikor kell ébreszteni             tcb+ 112 */
    uint64_t swapid;            /* swap azonosító, ha hibernált      tcb+ 120 */
    char *tracebuf;             /* nyomkövetés buffere               tcb+ 128 */
} __attribute__((packed)) tcb_t;

#endif
