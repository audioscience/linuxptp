
linux_src_path = ..
win_src_path = local

linux_srcs =  \
	bmc.c fsm.c msg.c pi.c ptp4l.c sk.c udp.c uds.c tlv.c \
	clock.c hwstamp_ctl.c phc.c port.c raw.c tmtab.c udp6.c \
	config.c mave.c phc2sys.c print.c servo.c transport.c util.c \
	bmc.h ds.h missing.h print.h tmtab.h util.h \
	clock.h ether.h msg.h raw.h tmv.h tlv.h \
	config.h fd.h pdt.h servo.h transport.h \
	contain.h foreign.h phc.h servo_private.h transport_private.h \
	ddt.h fsm.h pi.h sk.h udp.h \
	dm.h mave.h port.h tlv.h udp6.h uds.h
	
win_src = $(addprefix $(win_src_path)/,$(linux_srcs))

all: $(win_src)
	@echo AudioScience linux2win.mak source conversion completed.

$(win_src_path)/%.h: $(linux_src_path)/%.h linux2win.py
	@ python linux2win.py ${@F}

$(win_src_path)/%.c: $(linux_src_path)/%.c linux2win.py
	@ python linux2win.py ${@F}
