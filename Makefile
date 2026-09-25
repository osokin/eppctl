PROG=	eppctl
SRCS=	eppctl.c
MAN=	eppctl.8
CFLAGS+=	-Wall -Werror -Wextra -Wsign-compare
WARNS?=	6

.include <bsd.prog.mk>
