#CFLAGS = -O2 -Wall -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pie
CFLAGS = -O2 -Wall -D_FORTIFY_SOURCE=2 -fPIE -pie
LDFLAGS = -Wl,-z,relro,-z,now -Wl,-z,noexecstack

all: eduroam-test.cgi

eduroam-test.cgi: eduroam-test.c
	gcc $(CFLAGS) -o eduroam-test.cgi eduroam-test.c $(LDFLAGS) dict.c tcgi.c

install: eduroam-test.cgi
	cp -f eduroam-test.cgi /var/www/cgi-bin/
indent: eduroam-test.c
	indent eduroam-test.c -nbad -bap \
		-nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4  \
                -cli0 -d0 -di1 -nfc1 -i8 -ip0 -l100 -lp -npcs -nprs -npsl -sai \
                -saf -saw -ncs -nsc -sob -nfca -cp33 -ss -ts8 -il1
