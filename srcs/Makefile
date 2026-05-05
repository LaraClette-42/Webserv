SRC= main.cpp \
		server/Server.cpp \
		server/Client.cpp \
		server/Response.cpp \
		http/HttpRequest.cpp \
		http/HttpResponse.cpp \
		config/config.cpp \
		utils/utils.cpp
CPP= c++
FLAGS= -Wall -Wextra -Werror -std=c++98
RM= rm
NAME= webz

OBJ= ${SRC:.cpp=.o}

${NAME}: ${OBJ}
	@${CPP} ${FLAGS} -o ${NAME} ${OBJ}

%.o:%.cpp 
	${CPP} ${FLAGS} -c $< -o $@

all: ${NAME}

clean:
	@${RM} -rf ${OBJ}

fclean: clean
	@${RM} -f ${NAME} ${FILE}

re: fclean all

.PHONY: all re fclean clean