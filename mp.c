// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mp.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "buddy.h"

struct cpu cpus[NCPU];
int ncpu;
uchar ioapicid;

static uchar
/*
sum(uchar *addr, int len)
{
  int i, sum;

  sum = 0;
  for(i=0; i<len; i++)
    sum += addr[i];
  return sum;
}
*/
sum(uchar *addr, int length) {
    if (addr == 0 || length <= 0) {
        cprintf("Invalid address or length in sum\n");
        return -1;
    }

    int total = 0;
    for (int i = 0; i < length; i++) {
        total += addr[i];
    }
    return total;
}
// Look for an MP structure in the len bytes at addr.
static struct mp*
mpsearch1(uint a, int len)
{
  uchar *e, *p, *addr;

  addr = P2V(a);
  e = addr+len;
  for(p = addr; p < e; p += sizeof(struct mp))
    if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct mp*
mpsearch(void)
{
  uchar *bda;
  uint p;
  struct mp *mp;

  bda = (uchar *) P2V(0x400);
  if((p = ((bda[0x0F]<<8)| bda[0x0E]) << 4)){
    if((mp = mpsearch1(p, 1024)))
      return mp;
  } else {
    p = ((bda[0x14]<<8)|bda[0x13])*1024;
    if((mp = mpsearch1(p-1024, 1024)))
      return mp;
  }
  return mpsearch1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static struct mpconf*
mpconfig(struct mp **pmp)
{
  if (*pmp == 0 || (*pmp)->physaddr == 0 ) {
        return 0;
  }
  
  struct mp *mp = 0;
  struct mpconf *conf = (struct mpconf *)(mp->physaddr);

  mp = mpsearch();
if (mp == 0 || mp->physaddr == 0) {
    panic("Invalid MP structure or physaddr");
    return 0;
}

if (conf == 0) {
    panic("Failed to map MP configuration structure");
    return 0;
}

  if(mp == 0 || conf->length <= 0 || mp->physaddr == 0)
    return 0;
  conf = (struct mpconf*) P2V((uint) mp->physaddr);
  if(memcmp(conf, "PCMP", 4) != 0)
    return 0;
  if(conf->version != 1 && conf->version != 4)
    return 0;
  if(sum((uchar*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

/*
void
mpinit(void)
{
  uchar *p, *e;
  int ismp;
  struct mp *mp;
  struct mpconf *conf;
  struct mpproc *proc;
  struct mpioapic *ioapic;

  if((conf = mpconfig(&mp)) == 0)
    panic("Expect to run on an SMP");
  ismp = 1;
  lapic = (uint*)conf->lapicaddr;

if (p >= e) {
    cprintf("Invalid pointer bounds in MP initialization\n");
    return;
}
for (; p < e; p += sizeof(struct mp)) {
    switch(*p){
    case MPPROC:
      proc = (struct mpproc*)p;
      if(ncpu < NCPU) {
        cpus[ncpu].apicid = proc->apicid;  // apicid may differ from ncpu
        ncpu++;
      }
      p += sizeof(struct mpproc);
      continue;
    case MPIOAPIC:
      ioapic = (struct mpioapic*)p;
      ioapicid = ioapic->apicno;
      p += sizeof(struct mpioapic);
      continue;
    case MPBUS:
    case MPIOINTR:
    case MPLINTR:
      p += 8;
      continue;
    default:
      ismp = 0;
      break;
    }
  }
  if(!ismp)
    panic("Didn't find a suitable machine");

  if(mp->imcrp){
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
}
*/

void mpinit(void) {
    struct mp *pmp = mpsearch(); // Find the MP structure
    struct mpconf *conf; // Initialize 
    uchar *p = 0, *e = 0;

    if (pmp == 0) {
        cprintf("No MP structure found\n");
        return;
    }

    // Validate mp->physaddr before casting
    if (pmp->physaddr == 0 || (uintptr_t)pmp->physaddr > MAX_PHYS_ADDR) {
        cprintf("Invalid MP physical address\n");
        return;
    }

     conf = mpconfig(&pmp);

    if (conf == 0 || conf->length < sizeof(struct mpconf)) {
        cprintf("Invalid MP configuration\n");
        return;
    }

    // Validate conf version
    if (conf->version != 1 && conf->version != 4) {
        cprintf("Unsupported MP version: %d\n", conf->version);
        return;
    }

    // Checksum validation
    if (sum((uchar *)conf, conf->length) != 0) {
        cprintf("Invalid MP checksum\n");
        return;
    }

    lapic = (uint *)conf->lapicaddr;

    // Validate lapic address
    if (lapic == 0 || (uintptr_t)lapic > MAX_PHYS_ADDR) {
        cprintf("Invalid LAPIC address\n");
        return;
    }

    // Iterate through configuration entries
    p = (uchar *)(conf + 1);
    e = (uchar *)conf + conf->length;

    if (p >= e) {
        cprintf("Invalid MP configuration entries\n");
        return;
    }

    for (; p < e; p += sizeof(struct mp)) {
        // Process each entry
    }
}

