NAME		= webserv

CC			= c++
CFLAGS		= -Werror -Wextra -Wall -std=c++98

SRC_DIR		= src
OBJ_DIR		= obj
INC_DIR		= includes

SRCS		= main.cpp \
			  Config.cpp
OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
