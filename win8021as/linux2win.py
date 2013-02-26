# Code for translating linux PTP source into MSVC compatible source

usage = """linux2win.py output_file input_file
"""


import re
import os

def custom_print_h(code):
    # #include <syslog.h> -> a list of defines
    code = code.replace('#include <syslog.h>',"""
/* code defines stolen from syslog.h */

#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
""")
    # #define pr_emerg(x...)   print(LOG_EMERG, x) -> #define pr_emerg(x, ...)   print(LOG_EMERG, __VA_ARGS__)
    code = re.compile('(#define [^\(]*)\(x...\)').sub(r'\g<1>(...)',code)
    code = code.replace(', x)',', __VA_ARGS__)')
    # replace print prototype
    code = code.replace('void print(int level, char const *format, ...);','void print(int level, ...);');
    return code

def lin_to_win(fname, code):
    # PACKED -> #pragma pack(push,1)
    code = re.compile('(#define PACKED [^\n]*\n)').sub(r'',code)
    attrib_packed = re.compile('\n(struct[^}]*}) PACKED;')
    code = attrib_packed.sub(r'\n#pragma pack(push,1)\n\g<1>;\n#pragma pack(pop)\n', code)
    more_packed = re.compile('(struct[^}]*}) __attribute__\(\(packed\)\);')
    code = more_packed.sub(r'#pragma pack(push,1)\n\g<1>;\n#pragma pack(pop)\n', code)
    code = code.replace('inline ','_inline ')
    code = code.replace('#include <time.h>','#include "msvc_time.h"')
    code = code.replace('#include <poll.h>','#include "msvc_poll.h"')
    code = code.replace('#include <sys/syscall.h>','#include "msvc_syscall.h"')
    code = code.replace('#include <arpa/inet.h>','#include <winsock2.h>')
    code = code.replace('snprintf','_snprintf')
    code = code.replace('v_snprintf','_vsnprintf')
    code = code.replace('interface','net_interface')
    code = code.replace('enum {FALSE, TRUE};','#ifndef FALSE\nenum {FALSE, TRUE};\n#endif');
    code = code.replace('SIGQUIT','SIGBREAK');
   
    if fname == 'print.h':
        code = custom_print_h(code)
    elif fname == 'print.c':
        code = code.replace('#include <syslog.h>','')
        code = re.compile('(if \(use_syslog\) [^\}]*\})').sub(r'',code)
        code = code.replace('ptp4l[%ld.%03ld]: %s','ptp4win [%ld.%03ld]: %s')
    elif fname == 'contain.h':
        attrib_packed = re.compile('(#define container_of[^}]*}\))')
        code = attrib_packed.sub(r'#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))',code)
    elif fname == 'uds.c':
        code = code.replace('#include <sys/stat.h>','')
        code = code.replace('#include <sys/un.h>','')
        code = code.replace('#include <sys/socket.h>','#include <winsock2.h>')
    elif fname == 'missing.h':
        code = code.replace('#ifndef __uClinux__','#ifndef __uClinux__\nstatic _inline int timerfd_close(int clockid)\n{\nreturn syscall(__NR_timerfd_close, clockid);\n}\n')
        code = code.replace('#include <sys/timerfd.h>','')
        code = code.replace('#else','')
    elif fname == 'msg.c':
        code = code.replace('#include <asm/byteorder.h>','')
        code = code.replace('__cpu_to_be64(val)','(((unsigned __int64)htonl(val)) << 32) | ((unsigned __int64)(htonl(val >> 32)))')
        code = code.replace('__be64_to_cpu(val)','(((unsigned __int64)htonl(val)) << 32) | ((unsigned __int64)(htonl(val >> 32)))')
    elif fname == 'transport.c':
        # needed for NULL define
        code = code.replace('#include "transport.h"','#include <stdio.h>\n\n#include "transport.h"')
    elif fname == 'config.h':
        code = code.replace('#include "ds.h"','#include <unistd.h>\n#include "ds.h"')
    elif fname == 'config.c':
        code = code.replace('	Integer8 i8;','	Integer8 i8;\n	UInteger16 u16;')
        code = code.replace('unsigned char mac[MAC_LEN];','unsigned int mac[MAC_LEN];')
        code = code.replace('ptp_dst_mac %hhx:%hhx:%hhx:%hhx:%hhx:%hhx','ptp_dst_mac %x:%x:%x:%x:%x:%x')
        code = code.replace('p2p_dst_mac %hhx:%hhx:%hhx:%hhx:%hhx:%hhx','p2p_dst_mac %x:%x:%x:%x:%x:%x')
        code = code.replace('%hhu','%hc')
        code = code.replace('%hhd','%hc')
        code = code.replace('" clockAccuracy %hhx", &u8','" clockAccuracy %hx", &u16')
        code = code.replace('dds->clockQuality.clockAccuracy = u8','dds->clockQuality.clockAccuracy = u16')
        code = code.replace(' " transportSpecific %hhx", &u8',' " transportSpecific %hx", &u16')
        code = code.replace('pod->transportSpecific = u8 << 4','pod->transportSpecific = (uint8_t)u16 << 4')
    elif fname == 'port.c':
        code = code.replace('close(p->fda.fd[FD_ANNOUNCE_TIMER + i])','timerfd_close(p->fda.fd[FD_ANNOUNCE_TIMER + i])')

    return code



if __name__ == '__main__':
    from optparse import OptionParser

    parser = OptionParser(usage = usage)
    opts,args = parser.parse_args()

    print '* linux2win.py processing ' + args[0]

    if not os.path.exists('local'):
        os.makedirs('local')

    rd_file = open('..//'+args[0],'r');
    wr_file = open('local//'+args[0],'w');
    src = rd_file.read()
    src = lin_to_win(args[0], src)
    rd_file.close()
    wr_file.write(src)
    wr_file.close()
    
