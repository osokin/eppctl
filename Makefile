PROG=	eppctl
SRCS=	eppctl.c
MAN=	eppctl.8
CFLAGS+=	-Wextra -Wsign-compare
WARNS?=	6

.include <bsd.prog.mk>
