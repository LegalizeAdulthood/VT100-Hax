
#include "simthread.h"
#include <QFile>
#include "8080/sim.h"
#include "8080/simglb.h"
#include <stdio.h>

extern "C" {
extern void int_on(void), int_off(void);
extern void init_io(void), exit_io(void);

extern void cpu_z80(void), cpu_8080(void);
extern void disass(unsigned char **, int);
extern int exatoi(char *);
extern int getkey(void);
extern void int_on(void), int_off(void);
extern int load_file(char *);

static void do_step(void);
static void do_trace(char *);
static void do_go(char *);
static int handel_break(void);
static void do_dump(char *);
static void do_list(char *);
static void do_modify(char *);
static void do_fill(char *);
static void do_move(char *);
static void do_port(char *);
static void do_reg(char *);
static void print_head(void);
static void print_reg(void);
static void do_break(char *);
static void do_hist(char *);
static void do_count(char *);
static void do_clock(void);
static void timeout(int);
static void do_show(void);
static void do_unix(char *);
static void do_help(void);
static void cpu_err_msg(void);

}

SimThread* sim;

SimThread::SimThread(QObject *parent,char* romPath) :
    QThread(parent)
{
    this->romPath = romPath;
}

void SimThread::run() {
    i_flag = 1;
    f_flag = 10;
    m_flag = 0;
    tmax = f_flag*10000;
    cpu = I8080;
    quint8 leds = 0;
    printf("\nRelease %s, %s\n", RELEASE, COPYR);
    if (f_flag > 0)
        printf("\nCPU speed is %d MHz\n", f_flag);
    else
        printf("\nCPU speed is unlimited\n");
#ifdef	USR_COM
    printf("\n%s Release %s, %s\n", USR_COM, USR_REL, USR_CPR);
#endif
    fflush(stdout);

    printf("Prep ram\n");
    fflush(stdout);
    wrk_ram	= PC = ram;
    STACK = ram + 0xffff;
    if (cpu == I8080)	/* the unused flag bits are documented for */
        F = 2;		/* the 8080, so start with bit 1 set */
    memset((char *)	ram, m_flag, 65536);
    // load binary
    printf("Loading rom %s...\n",romPath);
    fflush(stdout);
    QFile romFile(romPath);
    if (!romFile.open(QIODevice::ReadOnly)) {
        printf("Failed to read rom file\n");
        fflush(stdout);
        return;
    }
    qint64 count = romFile.read((char*)ram,65536);
    romFile.close();
    int_on();
    // add local io hooks

    i_flag = 0;

    // We are always running the CPU in single-step mode so we can do the clock toggles when necessary.
    cpu_state = SINGLE_STEP;
    while (1) {
        while (stepsRemaining--) {
            cpu_error = NONE;
            cpu_8080();
            // Work out LBA7 signal (NVR clock)
            bool nlba7 = (t_ticks % 1024) < 100;
            if (!lba7 && nlba7) nvr.clock();
            lba7 = nlba7;
            //if (t_ticks - tstart)
        }
        msleep(50);
    }
    /*
    if (cpu_error == OPHALT)
        if (handel_break())
            if (!cpu_error)
                goto cont;
    cpu_err_msg();
    print_head();
    print_reg();
    */

    int_off();
}

BYTE SimThread::ioIn(BYTE addr) {
    if (addr == 0x42) {
        // Read buffer flag
        quint8 flags = 0x04;
        if (lba7) {
            flags |= 0x40;
        }
        if (nvr.output()) {
            flags |= 0x20;
        }
        return flags;
    } else {
        printf(" IN PORT %02x at %04x\n",addr,PC-ram);
        fflush(stdout);
    }
    return 0;
}

void SimThread::ioOut(BYTE addr, BYTE data) {
    switch(addr) {
    case 0x82:
        //printf("OUT PORT %02x <- %02x\n",addr,data);
        //fflush(stdout);
        emit outKbdStatus(data);
        break;
    case 0x62:
        //printf("NVRAM %02x\n",data);
        //fflush(stdout);
        nvr.set_latch(data);;
        break;
    default:
        printf("OUT PORT %02x <- %02x\n",addr,data);
        fflush(stdout);
    }
}

void SimThread::simStep(quint32 count)
{
    stepsRemaining = count;
}

void SimThread::simRun()
{
    stepsRemaining = 0xffffffff;
}

void SimThread::simStop()
{
    stepsRemaining = 0;
}

void SimThread::doSetup()
{
    printf("SET-UP\n");
    keypress(0x7b);
}

void SimThread::keypress(quint8 keycode)
{
    // todo
    // keyboard interrupt: int_data = 0xcf and int_int = 1

}

extern "C" {
BYTE io_in(BYTE addr);
void io_out(BYTE addr, BYTE data);
void exit_io();
}

void exit_io() {}

BYTE io_in(BYTE addr)
{
    return sim->ioIn(addr);
}

void io_out(BYTE addr, BYTE data)
{
    sim->ioOut(addr,data);
}
