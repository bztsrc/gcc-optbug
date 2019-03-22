/*
 * core/x86_64/ccb.h
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
 * @brief CPU Kontrol Blokk. architektúra függő struktúra
 *
 * !!!FIGYELEM!!! EGYBE KELL ESNIE A 64 BIT TSS-EL
 */

/* asmhez, egyeznie kell a cbb mezőivel */
#define ccb_flags       0x60
#define ccb_hasatask    0x61
#define ccb_core_errno  0x68

/* prioritási szintekért lásd src/core/task.h */
#ifndef _AS
typedef struct {
    uint32_t magic;         /* +00 (TSS64 offszet) CPU Kontrol Blokk azonosító 'CPUB' */
    uint64_t rsp0;          /* +04 */
    uint64_t rsp1;          /* +0C */
    uint64_t rsp2;          /* +14 */
    uint32_t apicid;        /* +1C valós APIC ID */
    uint32_t lapicid;       /* +20 logikai APIC ID */
    uint64_t ist1;          /* +24 felhasználó (kivételkezelő, IRQ) verem */
    uint64_t ist2;          /* +2C NMI verem */
    uint64_t ist3;          /* +34 debug verem */
    uint64_t ist4;          /* +3C nem használt */
    uint64_t ist5;          /* +44 nem használt */
    uint64_t ist6;          /* +4C nem használt */
    uint32_t id;            /* +54 CPU mag sorszám */
    phy_t sharedroot;       /* +58 megosztott memória címe, 512G, cr3[511] */
    uint8_t flags;          /* +60 taszkkapcsolás jelzők */
    uint8_t hasactivetask;  /* +61 van futtatható taszkja */
    uint16_t dummy[2];      /* +62 */
    uint16_t iomapbase;     /* +66 IO térkép címe, nem használt, nullának kell lennie */
    uint64_t core_errno;    /* +68 core hibakód */
    pid_t current;          /* +70 futásra kijelölt taszk */
    pid_t hd_timerq;        /* +78 időzítő sor fej (alarm) */
    pid_t hd_blocked;       /* blokkolt sor fej */
    pid_t hd_active[8];     /* prioritási sorok (aktív taszk fejek) */
    pid_t cr_active[8];     /* prioritási sorok (aktuális taszkok) */
    uint64_t sched_ticks;   /* ütemező számláló a következő riasztáshoz */
    uint64_t sched_delta;   /* ütemező számláló növelése */
    uint64_t numtasks;      /* erre a CPU-ra ütemezett taszkok száma */
    /* a fennmaradó részt a rendszer hívás használja CPU-nkénti veremnek */
} __attribute__((packed)) ccb_t;

#endif
