SRC=	srcs/main.cpp \
		srcs/server/Server.cpp \
		srcs/server/Client.cpp \
		srcs/http/HttpRequest.cpp \
		srcs/http/HttpResponse.cpp \
		srcs/config/config.cpp \
		srcs/utils/utils.cpp \
		srcs/cgi/CGI.cpp

CXX=	c++
CXXFLAGS=	-Wall -Wextra -Werror -std=c++98
RM=		rm
NAME=	webserv

OBJ=	${SRC:.cpp=.o}

${NAME}: ${OBJ}
	@${CXX} ${CXXFLAGS} -o ${NAME} ${OBJ}

%.o: %.cpp
	${CXX} ${CXXFLAGS} -c $< -o $@

all: ${NAME}

clean:
	@${RM} -rf ${OBJ}

fclean: clean
	@${RM} -f ${NAME}

re: fclean all

test:
	@bash tests/run_test.sh

.PHONY: all re fclean clean test
