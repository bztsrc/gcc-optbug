/*
 * core/sched.c
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
 * @brief Taszk ütemező
 */

/* --- intergrációs definíciók / definitions for integration --- */
typedef unsigned char       uint8_t;
typedef unsigned short int  uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long int   uint64_t;
typedef uint8_t bool_t;
typedef uint64_t pid_t;
typedef uint64_t phy_t;
#define __PAGEBITS      12
#define __PAGESIZE      (1UL<<__PAGEBITS)
#define PG_PAGE         0x0001
#define PG_CORE_RWNOCACHE 0x001b
#define RUNLVL_COOP     2
#define RUNLVL_NORM     4
#define OSZ_TCB_MAGICH  0x4B534154
#define LDYN_ccb        0xffffffff80000000
#define LDYN_tcbalarm   0xffffffff80001000
#define LDYN_tmpmap1    0xffffffff80004000
#define CCBS_ADDRESS    0xffffffffc0000000
#define true            1
#define false           0
#define EINVAL          1
#include "ccb.h"
#include "task.h"
uint8_t runlevel;
uint16_t numcores;
uint64_t quantum, clock_ticks;
tcb_t idle_tcb;
void vmm_page(phy_t mr, uint64_t v, phy_t p, uint64_t a);
void platform_awakecpu(uint16_t cpuid);
void intr_nextsched(uint64_t delta);
bool_t intr_nextschedremainder();
void drivers_ready();
void vmm_switch(phy_t phy);
void seterr(int);
void sched_pick();

/* --- eredeti sched.c / original sched.c --- */
#if DEBUG
extern char *prio[];
void sched_dump(uint16_t cpuid)
{
    tcb_t *tcb = (tcb_t*)LDYN_tmpmap3;
    ccb_t *ccb;
    uint i, j, m;
    pid_t p;

    if(cpuid == (uint16_t)-1) { j=0; m=numcores; } else { j=cpuid; m=(uint)cpuid+1; }
    for(; j<m; j++) {
        if(j>=numcores) break; /* plusz ellenőrzés, ha esetleg a debugger hibás cpuid-val hívta volna */
        ccb = (ccb_t*)(CCBS_ADDRESS + j*__PAGESIZE);
        if(cpuid==(uint16_t)-1) kprintf("------------- CPU #%d -------------\n", ccb->id);
        for(i=PRI_SYS; i<=PRI_IDLE; i++) {
            kprintf("%d %s hd %x:   ", i, prio[i], ccb->hd_active[i]);
            p = ccb->hd_active[i];
            while(p != 0) {
                vmm_page(0, LDYN_tmpmap3, p << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
                if(tcb->magic != OSZ_TCB_MAGICH) break;
                kprintf("%s%x%s(%x,%x) ", tcb->pid == ccb->cr_active[i]?">":"",
                    tcb->pid, tcb->pid == ccb->current?"*":"", tcb->prev, tcb->next);
                p = tcb->next;
            }
            kprintf("\n");
        }
        kprintf("blocked  hd %x:   ", ccb->hd_blocked);
        p = ccb->hd_blocked;
        while(p != 0) {
            vmm_page(0, LDYN_tmpmap3, p << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
            if(tcb->magic != OSZ_TCB_MAGICH) break;
            kprintf("%x%s(%x,%x) ", tcb->pid, tcb->pid == ccb->current?"*":"", tcb->prev, tcb->next);
            p = tcb->next;
        }
        kprintf("\n");
        kprintf("timerq (ticks %d delta %d) hd %x:   ", ccb->sched_ticks, ccb->sched_delta, ccb->hd_timerq);
        p = ccb->hd_timerq;
        while(p != 0) {
            vmm_page(0, LDYN_tmpmap3, p << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
            if(tcb->magic != OSZ_TCB_MAGICH) break;
            kprintf("%x%s(%d,%x,%x) ", tcb->pid, tcb->pid == ccb->current?"*":"", tcb->alarmusec, tcb->prev, tcb->next);
            p = tcb->next;
        }
        kprintf("\n");
    }
    kprintf("\n");
}
#endif

/**
 * ütemező megszakítás kezelője
 */
void sched_intr(uint16_t __attribute__((unused)) irq)
{
    /* ha az időzítő nem képes egyszerre egy egész usec-et várni, akkor több megszakítás is generálódhat */
    if(intr_nextschedremainder()) return;
    ((ccb_t*)LDYN_ccb)->sched_ticks += ((ccb_t*)LDYN_ccb)->sched_delta;
    sched_pick();
}

/**
 * egy taszk hozzáadása valamelyik aktív prioritási sorhoz
 */
void sched_add(tcb_t *tcb)
{
    ccb_t *ccb;
    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores) {
        seterr(EINVAL);
        return;
    }
    ccb = (ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE);
#if DEBUG
    if(debug&DBG_SCHED)
        kprintf("sched_add(%x) pri %d cpu %d\n", tcb->pid, tcb->priority, ccb->id);
#endif
    tcb->state = TCB_STATE_RUNNING;
    tcb->prev = 0;
    /* befűzés a megfelelő aktív sor elejére */
    tcb->next = ccb->hd_active[tcb->priority];
    ccb->hd_active[tcb->priority] = tcb->pid;
    if(tcb->next != 0) {
        vmm_page(0, LDYN_tmpmap1, tcb->next << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        ((tcb_t*)LDYN_tmpmap1)->prev = tcb->pid;
    }
}

/**
 * taszk kivétele prioritási sorból
 */
void sched_remove(tcb_t *tcb)
{
    ccb_t *ccb;
    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores) return;
    ccb = (ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE);
#if DEBUG
    if(debug&DBG_SCHED)
        kprintf("sched_remove(%x) pri %d cpu %d\n", tcb->pid, tcb->priority, ccb->id);
#endif
    /* kivesszük a sorok elejéről, ha ott van */
    if(ccb->hd_active[tcb->priority] == tcb->pid) {
        ccb->hd_active[tcb->priority] = tcb->next;
    }
    if(ccb->cr_active[tcb->priority] == tcb->pid) {
        ccb->cr_active[tcb->priority] = tcb->next;
    }
    if(ccb->hd_blocked == tcb->pid) {
        ccb->hd_blocked = tcb->next;
    }
    if(ccb->hd_timerq == tcb->pid) {
        ccb->hd_timerq = tcb->next;
    }
    /* kivesszük a láncolt listából */
    if(tcb->prev != 0) {
        vmm_page(0, LDYN_tmpmap1, tcb->prev << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        ((tcb_t*)LDYN_tmpmap1)->next = tcb->next;
    }
    if(tcb->next != 0) {
        vmm_page(0, LDYN_tmpmap1, tcb->next << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        ((tcb_t*)LDYN_tmpmap1)->prev = tcb->prev;
    }
    tcb->prev = tcb->next = 0;
}

/**
 * taszk mozgatása blokkolt vagy hibernált állapotból aktív sorba
 *  TCB_STATE_BLOCKED -> TCB_STATE_RUNNING
 */
void sched_awake(tcb_t *tcb)
{
    ccb_t *ccb;
    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores) return;
    ccb = (ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE);
    if(tcb->state != TCB_STATE_RUNNING) {
#if DEBUG
        if(debug&DBG_SCHED)
            kprintf("sched_awake(%x) cpu %d\n", tcb->pid, ccb->id);
#endif
        if(tcb->state == TCB_STATE_HYBERND) {
            /* TEENDŐ: swap -> memória */
            tcb->state = TCB_STATE_BLOCKED;
        }
        /* ccb->hd_blocked -> ccb->hd_active */
        tcb->blkcnt += clock_ticks - tcb->blktime;
        tcb->blktime = 0;
        /* kivesszük a blokkolt sor elejéről, ha ott van */
        if(ccb->hd_blocked == tcb->pid) {
            ccb->hd_blocked = tcb->next;
        }
        if(ccb->hd_timerq == tcb->pid) {
            tcb->alarmusec = 0;
            ccb->hd_timerq = tcb->next;
            if(ccb->hd_timerq)
                vmm_page(0, LDYN_tcbalarm, ccb->hd_timerq << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        }
        /* kivesszük a láncolt listából */
        if(tcb->prev != 0) {
            vmm_page(0, LDYN_tmpmap1, tcb->prev << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
            ((tcb_t*)LDYN_tmpmap1)->next = tcb->next;
        }
        if(tcb->next != 0) {
            vmm_page(0, LDYN_tmpmap1, tcb->next << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
            ((tcb_t*)LDYN_tmpmap1)->prev = tcb->prev;
        }
        sched_add(tcb);
        /* ha az adott cpu épp parkol, felébresztjük */
        if(ccb->id != ((ccb_t*)LDYN_ccb)->id && ccb->current == idle_tcb.pid)
            platform_awakecpu(ccb->id);
    }
    /* beállítjuk, hogy a most felébresztett taszk legyen a következő az adott prioritási sorban */
    ccb->cr_active[tcb->priority] = tcb->pid;
    ccb->hasactivetask = true;
}

/**
 * taszk átmozgatása aktív sorból blokkolt sorba
 *  TCB_STATE_RUNNING -> TCB_STATE_BLOCKED
 */
void sched_block(tcb_t *tcb)
{
    ccb_t *ccb;

    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores ||
        tcb->state != TCB_STATE_RUNNING) return;
    ccb = (ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE);
#if DEBUG
    if(debug&DBG_SCHED)
        kprintf("sched_block(%x) cpu %d\n", tcb->pid, ccb->id);
#endif
    tcb->state = TCB_STATE_BLOCKED;
    tcb->blktime = clock_ticks;
    /* ccb->hd_active -> ccb->hd_blocked */
    sched_remove(tcb);
    tcb->next = ccb->hd_blocked;
    tcb->prev = 0;
    /* befűzés a blokkolt sor elejére */
    if(ccb->hd_blocked) {
        vmm_page(0, LDYN_tmpmap1, ccb->hd_blocked << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        ((tcb_t*)LDYN_tmpmap1)->prev = tcb->pid;
    }
    ccb->hd_blocked = tcb->pid;
}

/**
 * blokkolt sorból háttértárolóra küld egy taszkot
 *  TCB_STATE_BLOCKED -> TCB_STATE_HYBERND
 */
void sched_hybernate(tcb_t *tcb)
{
    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores ||
        tcb->state != TCB_STATE_BLOCKED) return;
#if DEBUG
    if(debug&DBG_SCHED)
        kprintf("sched_hybernate(%x)\n", tcb->pid);
#endif
    /* TEENDŐ: memória -> swap (kivéve a tcb) */
    tcb->state = TCB_STATE_HYBERND;
}

/**
 * a megadott mikroszekundumig blokkolja a taszkot
 */
void sched_delay(tcb_t *tcb, uint64_t usec)
{
    pid_t next, prev;
    tcb_t *tcba = (tcb_t*)LDYN_tmpmap1;
    ccb_t *ccb;

    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores ||
        tcb->state != TCB_STATE_RUNNING) return;
    ccb = (ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE);
#if DEBUG
    if(debug&DBG_SCHED)
        kprintf("sched_delay(%x,%d)\n", tcb->pid, usec);
#endif
    tcb->state = TCB_STATE_BLOCKED;
    /* ccb->hd_active -> ccb->hd_timerq */
    sched_remove(tcb);
    if(!ccb->hd_timerq) ccb->sched_ticks = 0;
    tcb->alarmusec = ccb->sched_ticks + usec;
    /* befűzés a riasztásra várakozó sorba */
    next = ccb->hd_timerq;
    prev = 0;
    while(next) {
        vmm_page(0, LDYN_tmpmap1, next << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        if(tcb->alarmusec < tcba->alarmusec) break;
        prev = next;
        next = tcba->next;
    }
    tcb->next = next;
    if(prev) {
        /* ha volt taszk előttünk a sorban */
        vmm_page(0, LDYN_tmpmap1, prev << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        tcba->next = tcb->pid;
    } else {
        /* ha a sor elejére kerültünk */
        vmm_page(0, LDYN_tcbalarm, tcb->pid << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        ccb->hd_timerq = tcb->pid;
    }
}

/**
 * taszk átmozgatása egy másik processzor magra
 */
void sched_migrate(tcb_t *tcb, uint16_t cpuid)
{
    ccb_t *ccb;
    uint64_t usec;

    /* paraméterek ellenőrzése */
    if(tcb->magic != OSZ_TCB_MAGICH || tcb->priority > PRI_IDLE || tcb->cpuid >= numcores || cpuid >= numcores) return;
#if DEBUG
    if(debug&DBG_SCHED)
        kprintf("sched_migrate(%x) cpu %d -> %d\n", tcb->pid, tcb->cpuid, cpuid);
#endif
    usec = ((ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE))->sched_ticks;         /* régi CPU időzítő számlálója */
    sched_remove(tcb);                                                              /* kivesszük az ütemezőből */
    tcb->cpuid = cpuid;                                                             /* átállítjuk a CPU mag számát */
    switch(tcb->state) {                                                            /* beütemezzük az új CPU magon */
        case TCB_STATE_HYBERND: break;                                              /* hibernált, nincs dolgunk vele */
        case TCB_STATE_BLOCKED:
            if(tcb->alarmusec) {                                                    /* ha időzítésre várakozik */
                tcb->state = TCB_STATE_RUNNING;
                sched_delay(tcb, tcb->alarmusec - usec);
            } else {                                                                /* ha B/K-ra várakozik */
                ccb = (ccb_t*)(CCBS_ADDRESS + tcb->cpuid * __PAGESIZE);
                tcb->next = ccb->hd_blocked;
                tcb->prev = 0;
                if(ccb->hd_blocked) {
                    vmm_page(0, LDYN_tmpmap1, ccb->hd_blocked << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
                    ((tcb_t*)LDYN_tmpmap1)->prev = tcb->pid;
                }
                ccb->hd_blocked = tcb->pid;
            }
            break;
        case TCB_STATE_RUNNING:                                                     /* aktív sorba illesztés */
        default: sched_add(tcb); break;
    }
}

/**
 * a következő taszk kiválasztása futásra. Ez egy agyonoptimalizált,
 * O(1)-es, rekurzív prioritáslistás ütemező
 */
void sched_pick()
{
    volatile tcb_t *tcb = (tcb_t*)LDYN_tmpmap1, *tcba = (tcb_t*)LDYN_tcbalarm;
    volatile ccb_t *ccb = (ccb_t*)LDYN_ccb;
    uint64_t i, nonempty;

    /* következő taszk keresése az aktív sorokban. Ez a ciklus nem függ a taszkok számától */
    do {
        for(nonempty=false,i=PRI_SYS; i<PRI_IDLE; i++) {
            if(ccb->hd_timerq && i==tcba->priority && tcba->alarmusec <= ccb->sched_ticks) {
                sched_awake((tcb_t*)tcba);
                goto found;
            }
            if(!ccb->hd_active[i]) continue;
            nonempty = true;
            if(!ccb->cr_active[i]) {
                ccb->cr_active[i] = ccb->hd_active[i];
            } else
                goto found;
        }
    } while(nonempty);
    /* ha nem volt taszk egyik aktív sorban sem, akkor jöhet az idle sor */
    i = PRI_IDLE;
    if(ccb->cr_active[i] == 0)
        ccb->cr_active[i] = ccb->hd_active[i];
    /* ha ez a sor is üres, akkor az idle taszk marad, ami nincs benne egyik sorban sem */
    if(ccb->hd_active[i] == 0) {
        /* kooperatív ütemezésnél ez azt jelenti, minden eszközmeghajtó blokkolódott üzenetre várva */
        if(runlevel == RUNLVL_COOP) drivers_ready();
        ccb->hasactivetask = false;
        tcb = &idle_tcb;
    } else {
found:
        vmm_page(0, LDYN_tmpmap1, ccb->cr_active[i] << __PAGEBITS, PG_CORE_RWNOCACHE|PG_PAGE);
        ccb->cr_active[i] = tcb->next;
    }
    if(ccb->current != tcb->pid) {
#if DEBUG
        if(debug&DBG_SCHED)
            kprintf("sched_pick(cpu=%d)=%x  \n", ccb->id, tcb->pid);
#endif
        vmm_switch(tcb->memroot);
        ccb->current = tcb->pid;
        ccb->flags = 1;
    }
    /* következő ütemezés időzítése */
    if(runlevel == RUNLVL_NORM) {
        ccb->sched_delta = 0;
        /* ha a riasztási sor nem üres, és a mostani taszk megszakítható, vagy a riasztandó magasabb prioritású */
        if(ccb->hd_timerq && (tcb->priority > PRI_DRV || tcba->priority <= tcb->priority))
            ccb->sched_delta = tcba->alarmusec - ccb->sched_ticks;
        /* ha a mostani taszk megszakítható, és nem az idle taszk, akkor legkésőbb quantum nszek múlva meg is kell szakítani */
        if(tcb->priority > PRI_DRV && tcb != &idle_tcb && (!ccb->sched_delta || ccb->sched_delta > quantum))
            ccb->sched_delta = quantum;
        if(ccb->sched_delta)
            intr_nextsched(ccb->sched_delta);
    }
}
